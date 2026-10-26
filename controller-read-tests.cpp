// Exercise the actual external-read path with deterministic simulated memory.
// Does not open a driver, load a game, or create an overlay.
#include "controller.cpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>

struct FakeReader {
    mutable std::map<std::uint64_t, std::vector<unsigned char>> memory;
    mutable unsigned calls = 0;
    unsigned changeAt = 0;
    std::uint64_t changeAddress = 0, changeValue = 0;
    template<class T> void Put(std::uint64_t address, const T& value) const {
        auto& bytes = memory[address]; bytes.resize(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));
    }
    template<class T> bool Read(std::uint64_t address, T& value) const {
        if (++calls == changeAt) Put(changeAddress, changeValue);
        auto it = memory.find(address);
        if (it == memory.end() || it->second.size() != sizeof(T)) return false;
        std::memcpy(&value, it->second.data(), sizeof(T)); return true;
    }
};
constexpr std::uint64_t Module = 0x140000000, Game = 0x100000, Controller = 0x200000;
constexpr std::uint64_t Entity = 0x300000, ControllerBlock = 0x400000, EntityBlock = 0x500000;
constexpr std::uint64_t Mesh = 0x600000, MatrixOwner = 0x700000, MatrixBase = 0x800000;
constexpr auto Slot = Module + Config::GameBase;
FakeReader Fixture() {
    FakeReader r;
    r.Put(Slot, Game);
    r.Put(Game + Config::LocalPlayerController, ControllerBlock);
    r.Put(ControllerBlock, Controller + 0x10);
    r.Put(Controller + Config::ControlledEntity, EntityBlock);
    r.Put(EntityBlock, Entity + 0x10);
    r.Put(Entity + Config::EntityWorldPosition, Vector3{123, -45, 67});
    return r;
}
FakeReader HeadFixture() {
    FakeReader r = Fixture();
    r.Put(Entity + Config::MeshObjectComponent, Mesh);
    r.Put(Mesh + 0x30, MatrixOwner);
    r.Put(MatrixOwner + 0x10, MatrixBase);
    Matrix3x4 bone{{1,0,0}, {0,1,0}, {0,0,1}, {0.25f,1.75f,-0.5f}};
    r.Put(MatrixBase + std::uint64_t{Config::HeadBoneIndexCandidate} * Config::BoneMatrixStride, bone);
    Matrix3x4 entityTransform{{0,1,0}, {-1,0,0}, {0,0,1}, {10,20,30}};
    r.Put(Entity + Config::EntityTransform, entityTransform);
    return r;
}

void PutClass(FakeReader& r, std::uint64_t entity, std::uint64_t chainBase,
              const char* className) {
    const std::uint64_t first = chainBase;
    const std::uint64_t second = chainBase + 0x100;
    const std::uint64_t text = chainBase + 0x200;
    r.Put(entity + 0xE8, first);
    r.Put(first + 0x08, second);
    r.Put(second + 0x10, text);
    std::array<unsigned char, 96> bytes{};
    const std::size_t length = (std::min)(std::strlen(className), bytes.size() - 1);
    std::memcpy(bytes.data(), className, length);
    r.Put(text, bytes);
}
int main() {
    int failures = 0;
    auto check = [&](const char* name, const FakeReader& r, bool expected) {
        Vector3 out{9,9,9};
        bool ok = FetchLocalEntityPosition(r, Module, out);
        bool pass = ok == expected && (ok ? (out.x == 123 && out.y == -45 && out.z == 67)
            : (out.x == 9 && out.y == 9 && out.z == 9));
        std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
        if (!pass) ++failures;
    };
    check("world translation via two adjusted weak references", Fixture(), true);
    auto r = Fixture(); r.Put(Slot, std::uint64_t{0}); check("null game", r, false);
    r = Fixture(); r.Put(Game + Config::LocalPlayerController, std::uint64_t{0}); check("null controller", r, false);
    r = Fixture(); r.Put(Controller + Config::ControlledEntity, std::uint64_t{0}); check("null entity", r, false);
    r = Fixture(); r.Put(EntityBlock, std::uint64_t{8}); check("interior subtraction underflow", r, false);
    r = Fixture(); r.Put(EntityBlock, std::uint64_t{0xffff800000000010ull}); check("kernel pointer", r, false);
    r = Fixture(); r.memory.erase(Entity + Config::EntityWorldPosition); check("failed position read", r, false);
    r = Fixture(); r.Put(Entity + Config::EntityWorldPosition, Vector3{NAN,0,0}); check("nonfinite position", r, false);
    r = Fixture(); r.changeAt = 7; r.changeAddress = Slot; r.changeValue = Game + 0x1000;
    check("root transition", r, false);
    r = Fixture(); r.changeAt = 8; r.changeAddress = Game + Config::LocalPlayerController; r.changeValue = 0;
    check("controller transition", r, false);
    r = Fixture(); r.changeAt = 9; r.changeAddress = ControllerBlock; r.changeValue = Controller + 0x20;
    check("controller interior transition", r, false);
    r = Fixture(); r.changeAt = 10; r.changeAddress = Controller + Config::ControlledEntity; r.changeValue = 0;
    check("entity transition", r, false);

    {
        r = HeadFixture();
        HeadBoneProbe head{};
        const bool ok = FetchLocalHeadBoneCandidate(r, Module, head);
        const bool pass = ok && head.entity == Entity && head.matrixBase == MatrixBase
            && head.local.x == 0.25f && head.local.y == 1.75f && head.local.z == -0.5f
            && head.world.x == 8.25f && head.world.y == 20.25f && head.world.z == 29.5f;
        std::printf("%s head candidate primary matrix path and entity transform\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        r = HeadFixture();
        constexpr std::uint64_t MeshLink = 0x900000, MeshData = 0xA00000, Skeleton = 0xB00000;
        r.Put(Mesh + 0x30, std::uint64_t{0});
        r.Put(Mesh + 0x10, MeshLink);
        r.Put(MeshLink + 0x30, MeshData);
        r.Put(MeshData + 0xD0, Skeleton);
        r.Put(Skeleton + 0x10, MatrixBase);
        HeadBoneProbe head{};
        const bool pass = FetchLocalHeadBoneCandidate(r, Module, head)
            && head.matrixBase == MatrixBase && head.world.x == 8.25f;
        std::printf("%s head candidate fallback matrix path\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        r = HeadFixture();
        r.Put(MatrixOwner + 0x10, std::uint64_t{0});
        HeadBoneProbe head{1,2,3,{4,5,6},{7,8,9}};
        const bool ok = FetchLocalHeadBoneCandidate(r, Module, head);
        const bool pass = !ok && head.entity == 1 && head.world.x == 7;
        std::printf("%s head probe rejects null matrix base without publishing\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }

    {
        Camera c{{0,0,0},{1,0,0},{0,1,0},{0,0,1},1.0f};
        float x{}, y{};
        const bool center = WorldToScreen({0,0,10}, c, 1920, 1080, x, y)
            && x == 960.0f && y == 540.0f;
        const bool horizontal = WorldToScreen({16,0,10}, c, 1920, 1080, x, y)
            && std::abs(x - 1824.0f) < 0.01f;
        const bool vertical = WorldToScreen({0,9,10}, c, 1920, 1080, x, y)
            && std::abs(y - 54.0f) < 0.01f;
        const bool pass = center && horizontal && vertical;
        std::printf("%s binary-proven vertical-FOV projection\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const bool pass = std::abs(DistanceFadeFactor(0.0, 500.0f) - 1.0f) < 0.0001f
            && std::abs(DistanceFadeFactor(500.0, 500.0f) - 0.45f) < 0.0001f
            && DistanceFadeFactor(1000.0, 500.0f) == DistanceFadeFactor(500.0, 500.0f);
        std::printf("%s bounded distance color fading\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const bool pass = std::abs(CameraHeadingDegrees({0,0,1}) - 0.0) < 0.0001
            && std::abs(CameraHeadingDegrees({1,0,0}) - 90.0) < 0.0001
            && std::abs(CameraHeadingDegrees({0,0,-1}) - 180.0) < 0.0001
            && std::abs(CameraHeadingDegrees({-1,0,0}) - 270.0) < 0.0001;
        std::printf("%s camera heading uses north-zero clockwise convention\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const bool pass = ClassifyFrameTime(7.999) == FrameTimeBand::Green
            && ClassifyFrameTime(8.0) == FrameTimeBand::Yellow
            && ClassifyFrameTime(16.699) == FrameTimeBand::Yellow
            && ClassifyFrameTime(16.7) == FrameTimeBand::Red;
        std::printf("%s frame-time color thresholds\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const double samples[]{10.0, 20.0, 30.0, NAN};
        const FrameWindowSummary summary = SummarizeFrameWindow(samples, _countof(samples));
        const bool pass = std::abs(summary.maximum - 30.0) < 0.0001
            && std::abs(summary.average - 20.0) < 0.0001;
        std::printf("%s rolling frame window max/average ignores NaN\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        wchar_t temporary[MAX_PATH]{};
        const DWORD length = GetTempPathW(static_cast<DWORD>(_countof(temporary)), temporary);
        std::wstring path = length && length < _countof(temporary)
            ? std::wstring(temporary) + L"controller-config-roundtrip-test.ini"
            : L"controller-config-roundtrip-test.ini";
        DeleteFileW(path.c_str());
        PersistentConfig written{};
        written.showHealthBar = false;
        written.showDistanceText = false;
        written.showAiLabel = true;
        written.renderLocalPlayer = true;
        written.verticalHealthBar = true;
        written.distanceFade = false;
        written.showDebugHud = false;
        written.showSkeleton = true;
        written.showGmCamera = true;
        written.showSnaplines = true;
        written.showPlayerNames = false;
        written.showAlive = false;
        written.showDead = true;
        written.showUS = false;
        written.showUSSR = false;
        written.showFIA = false;
        written.showCIV = false;
        written.boxMode = 2;
        written.maxDistance = 742.5f;
        const bool saved = SaveConfigFile(path.c_str(), written);
        const PersistentConfig loaded = LoadConfigFile(path.c_str(), PersistentConfig{});
        DeleteFileW(path.c_str());
        const bool pass = saved && !loaded.showHealthBar && !loaded.showDistanceText
            && loaded.showAiLabel && loaded.renderLocalPlayer && loaded.verticalHealthBar
            && !loaded.distanceFade && !loaded.showDebugHud && loaded.maxDistance == 742.5f
            && loaded.showSkeleton && loaded.showGmCamera && loaded.showSnaplines
            && !loaded.showPlayerNames && !loaded.showAlive && loaded.showDead
            && !loaded.showUS && !loaded.showUSSR && !loaded.showFIA && !loaded.showCIV
            && loaded.boxMode == 2;
        std::printf("%s configuration INI round trip\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const SessionLabelText labels = MakeSessionLabelText(125);
        const bool pass = std::strcmp(SessionCredit,
            "https://github.com/KoreanLandGiant") == 0
            && std::strcmp(labels.uptime, "UPTIME  02:05") == 0
            && std::strcmp(labels.gameVersion, "ARMA REFORGER  1.8.0.13") == 0
            && std::strstr(labels.uptime, "reads") == nullptr
            && std::strstr(labels.gameVersion, "controller") == nullptr;
        std::printf("%s terminal watermark content\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        constexpr std::uint64_t CameraObject = 0x1500000;
        r = Fixture();
        CameraReadBlock block{};
        block.right = {1,0,0}; block.up = {0,1,0}; block.forward = {0,0,1};
        block.position = {10,20,30}; block.zoomScale = 0.75f;
        r.Put(CameraObject + 0x80, block);
        Camera camera{};
        const bool valid = ReadCameraBlock(r, CameraObject, camera)
            && camera.position.x == 10.0f && camera.forward.z == 1.0f
            && camera.zoomScale == 0.75f;
        block.right.x = NAN;
        r.Put(CameraObject + 0x80, block);
        Camera unchanged{{9,9,9},{8,8,8},{7,7,7},{6,6,6},5.0f};
        const bool nanRejected = !ReadCameraBlock(r, CameraObject, unchanged)
            && unchanged.position.x == 9.0f && unchanged.zoomScale == 5.0f;
        block.right = {1,0,0};
        block.position = {};
        r.Put(CameraObject + 0x80, block);
        const bool zeroRejected = !ReadCameraBlock(r, CameraObject, unchanged)
            && unchanged.position.x == 9.0f && unchanged.zoomScale == 5.0f;
        block.position = {10,20,30};
        block.zoomScale = NAN;
        r.Put(CameraObject + 0x80, block);
        const bool zoomRejected = !ReadCameraBlock(r, CameraObject, unchanged)
            && unchanged.position.x == 9.0f && unchanged.zoomScale == 5.0f;
        const bool layout = sizeof(CameraReadBlock) == 0x118
            && offsetof(CameraReadBlock, right) == 0x00
            && offsetof(CameraReadBlock, up) == 0x0C
            && offsetof(CameraReadBlock, forward) == 0x18
            && offsetof(CameraReadBlock, position) == 0x24
            && offsetof(CameraReadBlock, zoomScale) == 0x114;
        const bool pass = valid && nanRejected && zeroRejected && zoomRejected && layout;
        std::printf("%s camera block layout, validation, and no partial publish\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        constexpr std::uint64_t Hitzone = 0x1600000;
        r = Fixture();
        r.Put(Hitzone + 0x38, HealthPair{100.0f, 25.0f});
        float current = 7.0f, maximum = 8.0f;
        const bool valid = ReadHealthFloats(r, Hitzone, current, maximum)
            && current == 25.0f && maximum == 100.0f;
        r.Put(Hitzone + 0x38, HealthPair{1000001.0f, 0.0f});
        current = 7.0f; maximum = 8.0f;
        const bool rangeRejected = !ReadHealthFloats(r, Hitzone, current, maximum)
            && current == 7.0f && maximum == 8.0f;
        r.Put(Hitzone + 0x38, HealthPair{100.0f, NAN});
        const bool nanRejected = !ReadHealthFloats(r, Hitzone, current, maximum)
            && current == 7.0f && maximum == 8.0f;
        const bool layout = sizeof(HealthPair) == 0x08
            && offsetof(HealthPair, maximum) == 0x00
            && offsetof(HealthPair, current) == 0x04;
        const bool pass = valid && rangeRejected && nanRejected && layout;
        std::printf("%s health pair block validation and no partial publish\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        constexpr std::uint64_t PlayerList = 0x1700000;
        constexpr std::uint32_t PlayerCount = 130;
        r = Fixture();
        std::array<std::uint64_t, Config::EntityPointerBatchSize> first{};
        std::array<std::uint64_t, Config::EntityPointerBatchSize> second{};
        first[3] = 0x1710000;
        second[5] = 0x1720000;
        r.Put(PlayerList, first);
        r.Put(PlayerList + sizeof(first), second);
        r.Put(PlayerList + sizeof(first) + sizeof(second), std::uint64_t{0x1730000});
        r.Put(PlayerList + sizeof(first) + sizeof(second) + sizeof(std::uint64_t),
              std::uint64_t{0x1740000});
        std::vector<std::uint64_t> players{0xDEADBEEF};
        const unsigned before = r.calls;
        const bool valid = ReadPlayerPointers(r, PlayerList, PlayerCount, players);
        const unsigned reads = r.calls - before;
        std::vector<std::uint64_t> unchanged{0xABCDEF};
        r.memory.erase(PlayerList + sizeof(first));
        const bool rejected = !ReadPlayerPointers(r, PlayerList, PlayerCount, unchanged)
            && unchanged.size() == 1 && unchanged[0] == 0xABCDEF;
        const bool pass = valid && reads == 4 && players.size() == PlayerCount
            && players[3] == 0x1710000 && players[69] == 0x1720000
            && players[128] == 0x1730000 && players[129] == 0x1740000 && rejected;
        std::printf("%s player pointer batches use 4 reads instead of 130 and publish atomically\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        r = Fixture();
        constexpr std::uint64_t Fw = 0x1100000, Fdata = 0x1101000, FactionText = 0x1102000;
        constexpr std::uint64_t Damage = 0x1200000, Holder = 0x1201000, Hitzone = 0x1202000;
        constexpr std::uint64_t PlayerManager = 0x1300000, PlayerList = 0x1301000;
        constexpr std::uint64_t PlayerObject = 0x1302000, NameKey = 0x1400000;
        constexpr std::uint64_t PlayerNameText = 0x1401000;
        r.Put(Entity + 0x178, Fw);
        r.Put(Fw, Fdata);
        r.Put(Fdata + 0x60, FactionText);
        std::array<unsigned char, 96> factionBytes{};
        std::memcpy(factionBytes.data(), "USSR", 4);
        r.Put(FactionText, factionBytes);
        r.Put(Entity + 0x130, Damage);
        r.Put(Damage + 0x60, Holder);
        r.Put(Holder, Hitzone);
        r.Put(Hitzone + 0x38, HealthPair{100.0f, 25.0f});
        r.Put(Game + 0x2D8, PlayerManager);
        r.Put(PlayerManager + 0x18, PlayerList);
        r.Put(PlayerManager + 0x24, std::uint32_t{1});
        r.Put(PlayerList, PlayerObject);
        r.Put(PlayerObject + 0x48, NameKey);
        r.Put(PlayerObject + 0x18, PlayerNameText);
        std::array<unsigned char, 96> nameBytes{};
        std::memcpy(nameBytes.data(), "TestPlayer", 10);
        r.Put(PlayerNameText, nameBytes);

        CharacterVisualMetadata metadata{};
        ReadHealthForOverlay(r, Entity, metadata);
        const auto players = ReadPlayerNameMap(r, Game);
        const auto player = players.find(NameKey);
        const bool pass = ReadFactionForOverlay(r, Entity) == "USSR"
            && metadata.healthValid && !metadata.dead
            && metadata.hp == 25.0f && metadata.maxHp == 100.0f
            && player != players.end() && player->second == "TestPlayer";
        std::printf("%s overlay faction, health, and player metadata chains\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        Camera c{{0,0,0},{1,0,0},{0,1,0},{0,0,1},1.0f};
        ScreenCharacter nearMarker{}, farMarker{};
        const bool projected = ProjectCharacterGeometry({0,0,10}, c, 1920, 1080, nearMarker)
            && ProjectCharacterGeometry({0,0,20}, c, 1920, 1080, farMarker);
        const float nearHeight = nearMarker.feetY - nearMarker.headY;
        const float farHeight = farMarker.feetY - farMarker.headY;
        const bool pass = projected && nearHeight > farHeight && farHeight > 0.0f;
        std::printf("%s projected character geometry shrinks with distance\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        constexpr std::uint64_t WorldObject = 0xC00000, Data = 0xD00000;
        constexpr std::uint64_t LocalWorldBlock = 0xE00000;
        constexpr std::uint64_t NearCharacter = 0x310000, FarCharacter = 0x320000;
        r = Fixture();
        r.Put(Game + Config::World, WorldObject);
        r.Put(WorldObject + Config::EntityArrayData, Data);
        r.Put(WorldObject + Config::EntityArrayCount, std::uint32_t{3});
        r.Put(WorldObject + Config::WorldLocalCharacter, LocalWorldBlock);
        r.Put(LocalWorldBlock, Entity + 0x10);
        r.Put(Data, Entity);
        r.Put(Data + 8, NearCharacter);
        r.Put(Data + 16, FarCharacter);
        r.Put(Entity + Config::EntityWorldPosition, Vector3{100,10,100});
        r.Put(NearCharacter + Config::EntityWorldPosition, Vector3{110,10,100});
        r.Put(FarCharacter + Config::EntityWorldPosition, Vector3{1000,10,100});
        PutClass(r, Entity, 0xF00000, Config::CharacterClassName);
        PutClass(r, NearCharacter, 0xF01000, Config::CharacterClassName);
        PutClass(r, FarCharacter, 0xF02000, Config::CharacterClassName);

        CharacterScanState state{};
        const bool first = RefreshCharacterEntities(r, Module, state);
        const bool firstPass = first && state.entities.size() == 2
            && state.entities[0] == Entity && state.entities[1] == NearCharacter
            && state.classCache.size() == 3;

        r.memory.erase(Entity + 0xE8);
        r.memory.erase(NearCharacter + 0xE8);
        r.memory.erase(FarCharacter + 0xE8);
        const bool cached = RefreshCharacterEntities(r, Module, state)
            && state.entities.size() == 2 && state.classCache.size() == 3;

        r.Put(WorldObject + Config::EntityArrayCount, std::uint32_t{10000});
        const bool rejected = !RefreshCharacterEntities(r, Module, state)
            && state.entities.size() == 2 && state.lastCount == 3;
        const bool pass = firstPass && cached && rejected;
        std::printf("%s throttled character scan cache, range filter, and count guard\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        constexpr std::uint64_t WorldObject = 0x1700000, Data = 0x1800000;
        constexpr std::uint64_t LocalWorldBlock = 0x1900000;
        constexpr std::uint32_t LiveSizedCount = 70000;
        r = Fixture();
        r.Put(Game + Config::World, WorldObject);
        r.Put(WorldObject + Config::EntityArrayData, Data);
        r.Put(WorldObject + Config::EntityArrayCount, LiveSizedCount);
        r.Put(WorldObject + Config::WorldLocalCharacter, LocalWorldBlock);
        r.Put(LocalWorldBlock, Entity + 0x10);
        r.Put(Entity + Config::EntityWorldPosition, Vector3{100,10,100});
        std::uint32_t index = 0;
        while (index + Config::EntityPointerBatchSize <= LiveSizedCount) {
            std::array<std::uint64_t, Config::EntityPointerBatchSize> emptyBatch{};
            r.Put(Data + std::uint64_t{index} * sizeof(std::uint64_t), emptyBatch);
            index += Config::EntityPointerBatchSize;
        }
        for (; index < LiveSizedCount; ++index)
            r.Put(Data + std::uint64_t{index} * sizeof(std::uint64_t), std::uint64_t{0});

        CharacterScanState state{};
        ScanDiagnosticSnapshot acceptedSnapshot{};
        const bool accepted = RefreshCharacterEntities(r, Module, state, &acceptedSnapshot)
            && acceptedSnapshot.published && acceptedSnapshot.rawWorldCount == LiveSizedCount
            && acceptedSnapshot.deduplicatedPointers == 0 && state.lastCount == LiveSizedCount;

        r.Put(WorldObject + Config::EntityArrayCount, Config::FullEntityScanLimit + 1);
        ScanDiagnosticSnapshot rejectedSnapshot{};
        const bool rejected = !RefreshCharacterEntities(r, Module, state, &rejectedSnapshot)
            && std::strcmp(rejectedSnapshot.rejection, "count_limit") == 0
            && state.lastCount == LiveSizedCount;
        const bool pass = accepted && rejected;
        std::printf("%s live-sized world count admitted while safety cap remains enforced\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        const PersistentConfig saved = CurrentConfig();
        CharacterVisualMetadata metadata{};
        metadata.faction = "US";
        metadata.healthValid = true;
        metadata.dead = false;
        Config::ShowUS = false;
        const bool factionBlocked = !CharacterMetadataEnabled(&metadata);
        Config::ShowUS = true;
        Config::ShowAlive = false;
        const bool aliveBlocked = !CharacterMetadataEnabled(&metadata);
        Config::ShowDead = true;
        metadata.dead = true;
        const bool deadAllowed = CharacterMetadataEnabled(&metadata);
        ApplyConfig(saved);
        const bool pass = factionBlocked && aliveBlocked && deadAllowed;
        std::printf("%s faction and alive/dead filter\n", pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        r = FakeReader{};
        constexpr std::uint64_t TestEntity = 0x3000000, TestMesh = 0x3100000;
        constexpr std::uint64_t TestOwner = 0x3200000, TestBase = 0x3300000;
        constexpr std::uint64_t TestData = 0x3400000, TestObject = 0x3500000;
        constexpr std::uint64_t TestSkeleton = 0x3600000;
        r.Put(TestEntity + Config::MeshObjectComponent, TestMesh);
        SkeletonMeshLinks links{};
        links.data = TestData;
        links.owner = TestOwner;
        r.Put(TestMesh + Config::MeshComponentData, links);
        r.Put(TestOwner + 0x10, TestBase);
        r.Put(TestData + 0x30, TestObject);
        r.Put(TestObject + 0xD0, TestSkeleton);
        r.Put(TestSkeleton + 0x34, std::uint32_t{231});
        const Matrix3x4 identity{{1,0,0},{0,1,0},{0,0,1},{10,20,30}};
        r.Put(TestEntity + Config::EntityTransform, identity);
        std::array<Matrix3x4, 74> first{};
        std::array<Matrix3x4, 7> second{};
        for (std::size_t i = 0; i < first.size(); ++i) {
            first[i] = {{1,0,0},{0,1,0},{0,0,1},{float(i + 10),1,0}};
        }
        for (std::size_t i = 0; i < second.size(); ++i)
            second[i] = {{1,0,0},{0,1,0},{0,0,1},{float(i + 195),1,0}};
        r.Put(TestBase + 10ull * 0x30, first);
        r.Put(TestBase + 195ull * 0x30, second);
        SkeletonWorldSample out{};
        const bool valid = FetchCharacterSkeleton(r, TestEntity, out)
            && out.joints[Head].x == 93.0f
            && out.joints[HandB].x == 211.0f && r.calls == 15;
        auto broken = r;
        broken.memory.erase(TestBase + 195ull * 0x30);
        SkeletonWorldSample sentinel{};
        sentinel.joints[Head] = {9,9,9};
        const bool noPartial = !FetchCharacterSkeleton(broken, TestEntity, sentinel)
            && sentinel.joints[Head].x == 9.0f;
        auto invalidMatrix = r;
        first[SkeletonIndices[Head] - 10].translation.x =
            std::numeric_limits<float>::quiet_NaN();
        invalidMatrix.Put(TestBase + 10ull * 0x30, first);
        SkeletonWorldSample matrixSentinel{};
        matrixSentinel.joints[Head] = {9,9,9};
        const bool rejectsMatrix = !FetchCharacterSkeleton(
            invalidMatrix, TestEntity, matrixSentinel)
            && matrixSentinel.joints[Head].x == 9.0f;
        auto invalidCount = r;
        invalidCount.Put(TestSkeleton + 0x34, std::uint32_t{201});
        const bool rejectsCount = !FetchCharacterSkeleton(
            invalidCount, TestEntity, matrixSentinel)
            && matrixSentinel.joints[Head].x == 9.0f;
        auto changingChain = r;
        changingChain.calls = 0;
        changingChain.changeAt = 10;
        changingChain.changeAddress = TestEntity + Config::MeshObjectComponent;
        changingChain.changeValue = TestMesh + 0x1000;
        const bool rejectsTransition = !FetchCharacterSkeleton(
            changingChain, TestEntity, matrixSentinel)
            && matrixSentinel.joints[Head].x == 9.0f;
        const bool pass = valid && noPartial && rejectsMatrix && rejectsCount
            && rejectsTransition;
        std::printf("%s bounded skeleton blocks and no partial publication\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    {
        r = FakeReader{};
        constexpr std::uint64_t Gm = 0x4000000, Components = 0x4100000;
        constexpr std::uint64_t Component = 0x4200000;
        r.Put(Gm + Config::GmActiveFlag, std::uint64_t{1});
        r.Put(Gm + 0x38, Components); r.Put(Gm + 0x44, std::uint32_t{20});
        r.Put(Components + 18ull * 8, Component);
        const Matrix3x4 transform{{1,0,0},{0,1,0},{0,0,1},{100,20,300}};
        r.Put(Gm + Config::EntityTransform, transform);
        GmComponentBlock block{};
        block.qw = 1.0f; block.position = {100,20,300};
        r.Put(Component + 0x68, block);
        GmCameraSample out{};
        const bool valid = FetchGmCamera(r, Gm, out)
            && out.position.x == 100.0f && out.forward.z == 1.0f;
        block.qw = 0.0f; r.Put(Component + 0x68, block);
        GmCameraSample fallback{};
        const bool fallbackValid = FetchGmCamera(r, Gm, fallback)
            && fallback.position.x == 100.0f && fallback.forward.z == 1.0f;
        auto inactive = r;
        inactive.Put(Gm + Config::GmActiveFlag, std::uint64_t{0});
        GmCameraSample inactiveSentinel{{9,9,9},{9,9,9}};
        const bool hidesInactive = !FetchGmCamera(inactive, Gm, inactiveSentinel)
            && inactiveSentinel.position.x == 9.0f;
        auto exitedDuringRead = r;
        exitedDuringRead.calls = 0;
        exitedDuringRead.changeAt = 7;
        exitedDuringRead.changeAddress = Gm + Config::GmActiveFlag;
        exitedDuringRead.changeValue = 0;
        GmCameraSample transitionSentinel{{9,9,9},{9,9,9}};
        const bool hidesTransition = !FetchGmCamera(
            exitedDuringRead, Gm, transitionSentinel)
            && transitionSentinel.position.x == 9.0f;
        auto broken = r;
        Matrix3x4 invalidTransform = transform;
        invalidTransform.forward.x = std::numeric_limits<float>::quiet_NaN();
        broken.Put(Gm + Config::EntityTransform, invalidTransform);
        GmCameraSample sentinel{{9,9,9},{9,9,9}};
        const bool noPartial = !FetchGmCamera(broken, Gm, sentinel)
            && sentinel.position.x == 9.0f;
        const bool pass = valid && fallbackValid && hidesInactive && hidesTransition
            && noPartial;
        std::printf("%s GM active flag, quaternion, transform fallback, and no partial publication\n",
            pass ? "PASS" : "FAIL");
        if (!pass) ++failures;
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
