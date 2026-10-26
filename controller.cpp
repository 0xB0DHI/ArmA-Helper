// ArmA Helper public overlay shell. Live data acquisition is intentionally
// left as a placeholder; the rendering and fake-memory fixtures remain.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_dx11.h"
#include "third_party/imgui/backends/imgui_impl_win32.h"
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static_assert(sizeof(void*) == 8, "Build the controller for x64.");

namespace Config {
// Supplied Ghidra report identifies Arma Reforger 1.8.0.13 by embedded strings.
// Runtime image/version matching remains to be validated; RVAs are build-specific.
constexpr wchar_t ProcessName[] = L"ArmaReforgerSteam.exe";
constexpr wchar_t WindowTitle[] = L"Arma Reforger";
constexpr char TargetGameVersion[] = "1.8.0.13";
// Module-relative root pointer slot supplied by the project offset map.
// Retained as a research fixture for the fake-memory tests.
constexpr std::uint64_t GameBase = 0x22CC978;
constexpr bool RootConfigured = true;
constexpr std::uint64_t LocalPlayerController = 0x310;
constexpr std::uint64_t ControlledEntity = 0x120;
constexpr std::uint64_t EntityWorldPosition = 0xA4;
constexpr std::uint64_t World = 0x130;
constexpr std::uint64_t EntityArrayData = 0x150;
constexpr std::uint64_t EntityArrayCount = 0x15C;
constexpr std::uint64_t WorldLocalCharacter = 0x3A0;
constexpr std::uint32_t MaxEntityCount = 2000;
constexpr std::uint32_t EntityDiagnosticLimit = 50;
constexpr std::uint32_t FullEntityScanLimit = 81920;
constexpr DWORD EntityScanIntervalMs = 1000;
constexpr std::uint32_t EntityCountJumpAbsolute = 2000;
constexpr std::uint32_t EntityPointerBatchSize = 64;
inline float CharacterDistanceLimit = 500.0f;
constexpr float CharacterHeight = 1.8f;
constexpr char CharacterClassName[] = "SCR_ChimeraCharacterClass";
inline bool ShowAiLabel = false;
inline bool ShowHealthBar = true;
inline bool ShowDistanceText = true;
inline bool renderLocalPlayer = false;
inline bool VerticalHealthBar = false;
inline bool DistanceFade = true;
inline bool ShowDebugHud = true;
inline bool ShowSkeleton = false;
inline bool ShowGmCamera = false;
inline bool ShowSnaplines = false;
inline bool ShowPlayerNames = true;
inline bool ShowAlive = true;
inline bool ShowDead = false;
inline bool ShowUS = true, ShowUSSR = true, ShowFIA = true, ShowCIV = true;
inline int BoxMode = 1; // 0=off, 1=corner, 2=full 2D.
constexpr std::uint64_t EntityTransform = 0x80;
constexpr std::uint64_t MeshObjectComponent = 0x50;
constexpr std::uint32_t HeadBoneIndexCandidate = 83;
constexpr std::uint64_t BoneMatrixStride = 0x30;
constexpr std::uint32_t SkeletonMinimumCount = 202;
constexpr float SkeletonDistanceLimit = 500.0f;
constexpr std::uint64_t MeshMatrixOwner = 0x30;
constexpr std::uint64_t MatrixOwnerArray = 0x10;
constexpr std::uint64_t MeshComponentData = 0x10;
constexpr std::uint64_t MeshDataObject = 0x30;
constexpr std::uint64_t MeshObjectSkeleton = 0xD0;
constexpr std::uint64_t SkeletonBoneCount = 0x34;
constexpr std::uint64_t GmComponents = 0x38;
constexpr std::uint64_t GmComponentCount = 0x44;
constexpr std::uint32_t GmPreferredMovementIndex = 18;
constexpr std::uint64_t GmMovementQuaternion = 0x68;
constexpr std::uint64_t GmActiveFlag = 0x100;
constexpr char GmClassName[] = "SCR_EditorManagerEntityClass";
}

struct PersistentConfig {
    bool showHealthBar = true;
    bool showDistanceText = true;
    bool showAiLabel = false;
    bool renderLocalPlayer = false;
    bool verticalHealthBar = false;
    bool distanceFade = true;
    bool showDebugHud = true;
    bool showSkeleton = false, showGmCamera = false, showSnaplines = false;
    bool showPlayerNames = true, showAlive = true, showDead = false;
    bool showUS = true, showUSSR = true, showFIA = true, showCIV = true;
    int boxMode = 1;
    float maxDistance = 500.0f;
};

PersistentConfig CurrentConfig() {
    return {Config::ShowHealthBar, Config::ShowDistanceText, Config::ShowAiLabel,
            Config::renderLocalPlayer, Config::VerticalHealthBar, Config::DistanceFade,
            Config::ShowDebugHud,
            Config::ShowSkeleton, Config::ShowGmCamera, Config::ShowSnaplines,
            Config::ShowPlayerNames, Config::ShowAlive, Config::ShowDead,
            Config::ShowUS, Config::ShowUSSR, Config::ShowFIA, Config::ShowCIV,
            Config::BoxMode, Config::CharacterDistanceLimit};
}
void ApplyConfig(const PersistentConfig& config) {
    Config::ShowHealthBar = config.showHealthBar;
    Config::ShowDistanceText = config.showDistanceText;
    Config::ShowAiLabel = config.showAiLabel;
    Config::renderLocalPlayer = config.renderLocalPlayer;
    Config::VerticalHealthBar = config.verticalHealthBar;
    Config::DistanceFade = config.distanceFade;
    Config::ShowDebugHud = config.showDebugHud;
    Config::ShowSkeleton = config.showSkeleton;
    Config::ShowGmCamera = config.showGmCamera;
    Config::ShowSnaplines = config.showSnaplines;
    Config::ShowPlayerNames = config.showPlayerNames;
    Config::ShowAlive = config.showAlive;
    Config::ShowDead = config.showDead;
    Config::ShowUS = config.showUS;
    Config::ShowUSSR = config.showUSSR;
    Config::ShowFIA = config.showFIA;
    Config::ShowCIV = config.showCIV;
    Config::BoxMode = (std::max)(0, (std::min)(config.boxMode, 2));
    Config::CharacterDistanceLimit = (std::max)(10.0f, (std::min)(config.maxDistance, 2000.0f));
}
bool ReadIniBool(const wchar_t* path, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(L"Overlay", key, fallback ? 1 : 0, path) != 0;
}
PersistentConfig LoadConfigFile(const wchar_t* path, const PersistentConfig& defaults) {
    PersistentConfig config = defaults;
    config.showHealthBar = ReadIniBool(path, L"ShowHealthBar", defaults.showHealthBar);
    config.showDistanceText = ReadIniBool(path, L"ShowDistanceText", defaults.showDistanceText);
    config.showAiLabel = ReadIniBool(path, L"ShowAiLabel", defaults.showAiLabel);
    config.renderLocalPlayer = ReadIniBool(path, L"RenderLocalPlayer", defaults.renderLocalPlayer);
    config.verticalHealthBar = ReadIniBool(path, L"VerticalHealthBar", defaults.verticalHealthBar);
    config.distanceFade = ReadIniBool(path, L"DistanceFade", defaults.distanceFade);
    config.showDebugHud = ReadIniBool(path, L"ShowDebugHud", defaults.showDebugHud);
    config.showSkeleton = ReadIniBool(path, L"ShowSkeleton", defaults.showSkeleton);
    config.showGmCamera = ReadIniBool(path, L"ShowGmCamera", defaults.showGmCamera);
    config.showSnaplines = ReadIniBool(path, L"ShowSnaplines", defaults.showSnaplines);
    config.showPlayerNames = ReadIniBool(path, L"ShowPlayerNames", defaults.showPlayerNames);
    config.showAlive = ReadIniBool(path, L"ShowAlive", defaults.showAlive);
    config.showDead = ReadIniBool(path, L"ShowDead", defaults.showDead);
    config.showUS = ReadIniBool(path, L"ShowUS", defaults.showUS);
    config.showUSSR = ReadIniBool(path, L"ShowUSSR", defaults.showUSSR);
    config.showFIA = ReadIniBool(path, L"ShowFIA", defaults.showFIA);
    config.showCIV = ReadIniBool(path, L"ShowCIV", defaults.showCIV);
    config.boxMode = (std::max)(0, (std::min)(static_cast<int>(GetPrivateProfileIntW(
        L"Overlay", L"BoxMode", defaults.boxMode, path)), 2));
    wchar_t distance[32]{};
    GetPrivateProfileStringW(L"Overlay", L"MaxDistance", L"", distance,
                             static_cast<DWORD>(_countof(distance)), path);
    if (distance[0]) {
        wchar_t* end = nullptr;
        const float parsed = std::wcstof(distance, &end);
        if (end && *end == L'\0' && std::isfinite(parsed)) config.maxDistance = parsed;
    }
    config.maxDistance = (std::max)(10.0f, (std::min)(config.maxDistance, 2000.0f));
    return config;
}
bool SaveConfigFile(const wchar_t* path, const PersistentConfig& config) {
    const auto writeBool = [&](const wchar_t* key, bool value) {
        return WritePrivateProfileStringW(L"Overlay", key, value ? L"1" : L"0", path) != FALSE;
    };
    wchar_t distance[32]{};
    swprintf_s(distance, L"%.1f", static_cast<double>(config.maxDistance));
    wchar_t boxMode[8]{};
    swprintf_s(boxMode, L"%d", config.boxMode);
    return writeBool(L"ShowHealthBar", config.showHealthBar)
        && writeBool(L"ShowDistanceText", config.showDistanceText)
        && writeBool(L"ShowAiLabel", config.showAiLabel)
        && writeBool(L"RenderLocalPlayer", config.renderLocalPlayer)
        && writeBool(L"VerticalHealthBar", config.verticalHealthBar)
        && writeBool(L"DistanceFade", config.distanceFade)
        && writeBool(L"ShowDebugHud", config.showDebugHud)
        && writeBool(L"ShowSkeleton", config.showSkeleton)
        && writeBool(L"ShowGmCamera", config.showGmCamera)
        && writeBool(L"ShowSnaplines", config.showSnaplines)
        && writeBool(L"ShowPlayerNames", config.showPlayerNames)
        && writeBool(L"ShowAlive", config.showAlive)
        && writeBool(L"ShowDead", config.showDead)
        && writeBool(L"ShowUS", config.showUS)
        && writeBool(L"ShowUSSR", config.showUSSR)
        && writeBool(L"ShowFIA", config.showFIA)
        && writeBool(L"ShowCIV", config.showCIV)
        && WritePrivateProfileStringW(L"Overlay", L"BoxMode", boxMode, path) != FALSE
        && WritePrivateProfileStringW(L"Overlay", L"MaxDistance", distance, path) != FALSE;
}
std::wstring ConfigFilePath() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(_countof(path)));
    if (!length || length >= _countof(path)) return L"controller.ini";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return L"controller.ini";
    slash[1] = L'\0';
    return std::wstring(path) + L"controller.ini";
}

void DiagnosticLog(const char* format, ...);

enum class ReadPhase { None, Frame, Scan };
struct ReadStatistics {
    ReadPhase phase = ReadPhase::None;
    std::uint64_t frameReads = 0;
    std::uint64_t frameReadFailures = 0;
    std::uint64_t scanReads = 0;
    std::uint64_t scanReadFailures = 0;
    std::uint64_t totalReads = 0;
    std::uint64_t totalReadFailures = 0;
    double frameReadMs = 0.0;
    double scanReadMs = 0.0;
    double scanMs = 0.0;
    double iterationMs = 0.0;
    double workMs = 0.0;
    double waitMs = 0.0;
    double presentMs = 0.0;
    double cameraAgeMs = 0.0;
    double frameDeltaMs = 0.0;
    double smoothedFps = 0.0;
    std::array<double, 60> frameTimes{};
    std::size_t frameTimeCount = 0;
    std::size_t frameTimeNext = 0;
    std::array<std::uint64_t, 180> readHistory{};
    std::size_t readHistoryCount = 0;
    std::size_t readHistoryNext = 0;
    std::uint64_t acceptedScans = 0;
    std::uint64_t rejectedScans = 0;
    std::uint64_t consecutiveRejectedScans = 0;
    std::uint64_t consecutiveFullScans = 0;
    std::uint32_t lastWorldCount = 0;
    bool lastScanPartial = false;
    ULONGLONG lastAcceptedScanTick = 0;
    std::uint64_t displayedTotalReads = 0;
    std::uint64_t displayedTotalReadFailures = 0;
    std::uint64_t displayedAcceptedScans = 0;
    std::uint64_t displayedRejectedScans = 0;
    ULONGLONG displayedUptimeMinutes = 0;
    LARGE_INTEGER iterationStart{};
    LARGE_INTEGER previousFrameStart{};
    LARGE_INTEGER scanStart{};
    LARGE_INTEGER cameraReadComplete{};
    LARGE_INTEGER frequency{};
    ULONGLONG sessionStartTick = 0;
    ULONGLONG nextSessionUpdate = 0;
    ULONGLONG nextLog = 0;
};
ReadStatistics g_readStatistics;

double CounterMilliseconds(LARGE_INTEGER start, LARGE_INTEGER end) {
    return double(end.QuadPart - start.QuadPart) * 1000.0
        / double(g_readStatistics.frequency.QuadPart);
}
void InitializeReadStatistics() {
    QueryPerformanceFrequency(&g_readStatistics.frequency);
    const ULONGLONG now = GetTickCount64();
    g_readStatistics.sessionStartTick = now;
    g_readStatistics.nextSessionUpdate = now;
    g_readStatistics.nextLog = now + 1000;
}
void BeginFrameStatistics() {
    LARGE_INTEGER nowCounter{};
    QueryPerformanceCounter(&nowCounter);
    if (g_readStatistics.previousFrameStart.QuadPart != 0) {
        g_readStatistics.frameDeltaMs = CounterMilliseconds(
            g_readStatistics.previousFrameStart, nowCounter);
        g_readStatistics.frameTimes[g_readStatistics.frameTimeNext] =
            g_readStatistics.frameDeltaMs;
        g_readStatistics.frameTimeNext = (g_readStatistics.frameTimeNext + 1)
            % g_readStatistics.frameTimes.size();
        g_readStatistics.frameTimeCount = (std::min)(g_readStatistics.frameTimeCount + 1,
            g_readStatistics.frameTimes.size());
        double smoothingTotal = 0.0;
        const std::size_t smoothingCount = (std::min)(std::size_t{8},
            g_readStatistics.frameTimeCount);
        for (std::size_t i = 0; i < smoothingCount; ++i) {
            const std::size_t index = (g_readStatistics.frameTimeNext
                + g_readStatistics.frameTimes.size() - 1 - i)
                % g_readStatistics.frameTimes.size();
            smoothingTotal += g_readStatistics.frameTimes[index];
        }
        g_readStatistics.smoothedFps = smoothingTotal > 0.0
            ? 1000.0 * double(smoothingCount) / smoothingTotal : 0.0;
        g_readStatistics.readHistory[g_readStatistics.readHistoryNext] =
            g_readStatistics.frameReads;
        g_readStatistics.readHistoryNext = (g_readStatistics.readHistoryNext + 1)
            % g_readStatistics.readHistory.size();
        g_readStatistics.readHistoryCount = (std::min)(g_readStatistics.readHistoryCount + 1,
            g_readStatistics.readHistory.size());
    }
    g_readStatistics.previousFrameStart = nowCounter;
    g_readStatistics.phase = ReadPhase::Frame;
    g_readStatistics.frameReads = 0;
    g_readStatistics.frameReadFailures = 0;
    g_readStatistics.frameReadMs = 0.0;
    g_readStatistics.iterationStart = nowCounter;
    const ULONGLONG now = GetTickCount64();
    if (now >= g_readStatistics.nextSessionUpdate) {
        g_readStatistics.displayedTotalReads = g_readStatistics.totalReads;
        g_readStatistics.displayedTotalReadFailures = g_readStatistics.totalReadFailures;
        g_readStatistics.displayedAcceptedScans = g_readStatistics.acceptedScans;
        g_readStatistics.displayedRejectedScans = g_readStatistics.rejectedScans;
        g_readStatistics.displayedUptimeMinutes =
            (now - g_readStatistics.sessionStartTick) / 60000;
        g_readStatistics.nextSessionUpdate = now + 1000;
    }
}
void BeginScanStatistics() {
    g_readStatistics.phase = ReadPhase::Scan;
    g_readStatistics.scanReads = 0;
    g_readStatistics.scanReadFailures = 0;
    g_readStatistics.scanReadMs = 0.0;
    QueryPerformanceCounter(&g_readStatistics.scanStart);
}
void EndScanStatistics() {
    LARGE_INTEGER end{};
    QueryPerformanceCounter(&end);
    g_readStatistics.scanMs = CounterMilliseconds(g_readStatistics.scanStart, end);
    g_readStatistics.phase = ReadPhase::Frame;
}
void EndFrameStatistics() {
    LARGE_INTEGER end{};
    QueryPerformanceCounter(&end);
    g_readStatistics.iterationMs = CounterMilliseconds(g_readStatistics.iterationStart, end);
    g_readStatistics.phase = ReadPhase::None;
    if (GetTickCount64() >= g_readStatistics.nextLog) {
        DiagnosticLog("[read-stats] frame_reads=%llu frame_failures=%llu frame_read_ms=%.3f "
            "scan_reads=%llu scan_failures=%llu scan_read_ms=%.3f scan_ms=%.3f "
            "work_ms=%.3f wait_ms=%.3f present_ms=%.3f iteration_ms=%.3f\n",
            static_cast<unsigned long long>(g_readStatistics.frameReads),
            static_cast<unsigned long long>(g_readStatistics.frameReadFailures),
            g_readStatistics.frameReadMs,
            static_cast<unsigned long long>(g_readStatistics.scanReads),
            static_cast<unsigned long long>(g_readStatistics.scanReadFailures),
            g_readStatistics.scanReadMs, g_readStatistics.scanMs,
            g_readStatistics.workMs, g_readStatistics.waitMs,
            g_readStatistics.presentMs,
            g_readStatistics.iterationMs);
        g_readStatistics.nextLog = GetTickCount64() + 1000;
    }
}
void MarkFrameWorkComplete() {
    LARGE_INTEGER end{};
    QueryPerformanceCounter(&end);
    g_readStatistics.workMs = CounterMilliseconds(g_readStatistics.iterationStart, end);
}
void RecordPresentTime(LARGE_INTEGER start, LARGE_INTEGER end) {
    g_readStatistics.presentMs = CounterMilliseconds(start, end);
}
void RecordMessageWaitTime(LARGE_INTEGER start, LARGE_INTEGER end) {
    g_readStatistics.waitMs = CounterMilliseconds(start, end);
}
void RecordCameraReadComplete() {
    QueryPerformanceCounter(&g_readStatistics.cameraReadComplete);
}
void RecordCameraAge(LARGE_INTEGER presentReturned) {
    if (g_readStatistics.cameraReadComplete.QuadPart != 0)
        g_readStatistics.cameraAgeMs = CounterMilliseconds(
            g_readStatistics.cameraReadComplete, presentReturned);
}
void RecordDriverRead(LARGE_INTEGER start, LARGE_INTEGER end, bool success, bool timed) {
    const double elapsed = timed ? CounterMilliseconds(start, end) : 0.0;
    ++g_readStatistics.totalReads;
    if (!success) ++g_readStatistics.totalReadFailures;
    if (g_readStatistics.phase == ReadPhase::Scan) {
        ++g_readStatistics.scanReads;
        if (!success) ++g_readStatistics.scanReadFailures;
        g_readStatistics.scanReadMs += elapsed;
    } else if (g_readStatistics.phase == ReadPhase::Frame) {
        ++g_readStatistics.frameReads;
        if (!success) ++g_readStatistics.frameReadFailures;
        g_readStatistics.frameReadMs += elapsed;
    }
}

DWORD FindProcess(const wchar_t*) {
    // Window APIs identify the target without accessing its memory.
    HWND window = FindWindowW(nullptr, Config::WindowTitle);
    DWORD pid = 0;
    if (window) GetWindowThreadProcessId(window, &pid);
    return pid;
}

bool AddAddress(std::uint64_t base, std::uint64_t offset, std::uint64_t& result) {
    if (!base || offset > (std::numeric_limits<std::uint64_t>::max)() - base) return false;
    result = base + offset;
    return true;
}

class PlaceholderReader {
public:
    template<class T>
    bool Read(std::uint64_t, T&,
              std::initializer_list<std::uint32_t> = {}) const {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(sizeof(T) <= 4096);
        // Supply a validated read-only data source in a private integration.
        return false;
    }
    DWORD LastError() const { return ERROR_CALL_NOT_IMPLEMENTED; }
};

struct Vector3 { float x, y, z; };
static_assert(sizeof(Vector3) == 12);
Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
double Dot(Vector3 a, Vector3 b) {
    return double(a.x)*b.x + double(a.y)*b.y + double(a.z)*b.z;
}
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
// Binary analysis establishes camera +0x194 as tan(vertical FOV / 2) for the
// symmetric projection written by SetVerticalFOV.
struct Camera { Vector3 position, right, up, forward; float zoomScale; };

enum class FrameTimeBand { Green, Yellow, Red };

FrameTimeBand ClassifyFrameTime(double milliseconds) {
    if (milliseconds < 8.0) return FrameTimeBand::Green;
    if (milliseconds < 16.7) return FrameTimeBand::Yellow;
    return FrameTimeBand::Red;
}

double CameraHeadingDegrees(Vector3 forward) {
    // X is east/west and Z is north/south: north (+Z) is 0 degrees and
    // atan2(X, Z) makes positive headings rotate clockwise when viewed from above.
    if (!Finite(forward)) return 0.0;
    constexpr double RadiansToDegrees = 57.295779513082320876;
    double heading = std::atan2(double(forward.x), double(forward.z)) * RadiansToDegrees;
    if (heading < 0.0) heading += 360.0;
    return heading;
}

struct FrameWindowSummary {
    double maximum = 0.0;
    double average = 0.0;
};

FrameWindowSummary SummarizeFrameWindow(const double* samples, std::size_t count) {
    FrameWindowSummary summary{};
    double total = 0.0;
    std::size_t validCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(samples[i])) continue;
        summary.maximum = (std::max)(summary.maximum, samples[i]);
        total += samples[i];
        ++validCount;
    }
    if (validCount) summary.average = total / double(validCount);
    return summary;
}

// Packed 3x4 transform used by both entity transforms and bone matrices.
struct Matrix3x4 { Vector3 right, up, forward, translation; };
static_assert(sizeof(Matrix3x4) == Config::BoneMatrixStride);

Vector3 TransformPoint(const Matrix3x4& transform, Vector3 point) {
    return {
        transform.right.x * point.x + transform.up.x * point.y
            + transform.forward.x * point.z + transform.translation.x,
        transform.right.y * point.x + transform.up.y * point.y
            + transform.forward.y * point.z + transform.translation.y,
        transform.right.z * point.x + transform.up.z * point.y
            + transform.forward.z * point.z + transform.translation.z
    };
}

bool Finite(const Matrix3x4& transform) {
    return Finite(transform.right) && Finite(transform.up)
        && Finite(transform.forward) && Finite(transform.translation);
}

// Confirmed for both native camera weak fields in the analyzed binary:
// field -> control block -> interior object pointer -> subtract 0x10.
// Control block +0x08 is a reference count, NOT an object pointer.
// Passing the field ADDRESS is essential; do not pass its pre-read value.
// Null checks detect unresolved references, but cannot prevent destruction
// between external reads or keep the resulting object alive.
struct WeakSnapshot {
    std::uint64_t field{}, controlBlock{}, interior{}, object{};
};
bool UserAddress(std::uint64_t address) {
    // x64 lower canonical range; actual mapping/access is checked by the driver.
    return address >= 0x10000 && address < 0x0000800000000000ull;
}
template<class Reader>
bool ReadWeakSnapshot(const Reader& reader, std::uint64_t field, WeakSnapshot& out) {
    out = {};
    WeakSnapshot s{}; s.field = field;
    if (!UserAddress(field) || !reader.Read(field, s.controlBlock) || !UserAddress(s.controlBlock)
        || !reader.Read(s.controlBlock, s.interior) || s.interior <= 0x10
        || !UserAddress(s.interior)) return false;
    s.object = s.interior - 0x10;
    if (!UserAddress(s.object)) return false;
    out = s;
    return true;
}
template<class Reader>
bool WeakUnchanged(const Reader& reader, const WeakSnapshot& s) {
    std::uint64_t cb{}, interior{};
    return reader.Read(s.field, cb) && cb == s.controlBlock
        && reader.Read(s.controlBlock, interior) && interior == s.interior;
}
bool ResolveWeak(const PlaceholderReader& reader, std::uint64_t weakField, std::uint64_t& object) {
    object = 0;
    WeakSnapshot s{};
    if (!ReadWeakSnapshot(reader, weakField, s)) return false;
    object = s.object;
    return true;
}

void DiagnosticLog(const char* format, ...) {
    char line[512]{};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf_s(line, sizeof(line), _TRUNCATE, format, arguments);
    va_end(arguments);
    OutputDebugStringA(line);
    std::fputs(line, stdout);
    std::fflush(stdout);
}

// Read-only, one-shot diagnostic for the candidate world entity array. Every
// Data acquisition is an inert placeholder in this public copy. The
// templated samplers below remain testable with FakeReader fixtures.
// Invalid pointers and failed/non-finite position reads are reported and skipped.
template<class Reader>
void LogEntityArrayDiagnostic(const Reader& reader, std::uint64_t moduleBase) {
    DiagnosticLog("[entity-diagnostic] begin\n");

    std::uint64_t rootSlot{}, game{};
    if (!Config::RootConfigured
        || !AddAddress(moduleBase, Config::GameBase, rootSlot)
        || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game)
        || !UserAddress(game)) {
        DiagnosticLog("[entity-diagnostic] game pointer unavailable or invalid\n");
        return;
    }

    std::uint64_t worldField{}, world{};
    if (!AddAddress(game, Config::World, worldField)
        || !UserAddress(worldField)
        || !reader.Read(worldField, world)
        || !UserAddress(world)) {
        DiagnosticLog("[entity-diagnostic] game=0x%llX world pointer unavailable or invalid\n",
            static_cast<unsigned long long>(game));
        return;
    }

    std::uint64_t localField{}, localCharacter{};
    WeakSnapshot localSnapshot{};
    const bool haveLocal = AddAddress(world, Config::WorldLocalCharacter, localField)
        && ReadWeakSnapshot(reader, localField, localSnapshot);
    if (haveLocal) localCharacter = localSnapshot.object;

    DiagnosticLog("[entity-diagnostic] game=0x%llX world=0x%llX local=0x%llX%s\n",
        static_cast<unsigned long long>(game),
        static_cast<unsigned long long>(world),
        static_cast<unsigned long long>(localCharacter),
        haveLocal ? "" : " (unresolved)");

    std::uint64_t dataField{}, countField{}, data{};
    std::uint32_t count{};
    if (!AddAddress(world, Config::EntityArrayData, dataField)
        || !AddAddress(world, Config::EntityArrayCount, countField)
        || !UserAddress(dataField) || !UserAddress(countField)
        || !reader.Read(dataField, data)
        || !reader.Read(countField, count)) {
        DiagnosticLog("[entity-diagnostic] entity array header read failed\n");
        return;
    }

    const std::uint32_t cappedCount = (std::min)(count, Config::MaxEntityCount);
    const std::uint32_t inspectCount = (std::min)(cappedCount, Config::EntityDiagnosticLimit);
    DiagnosticLog("[entity-diagnostic] total_count=%u capped_count=%u inspecting=%u data=0x%llX\n",
        count, cappedCount, inspectCount, static_cast<unsigned long long>(data));

    bool localInList = false;
    if (!UserAddress(data)) {
        DiagnosticLog("[entity-diagnostic] data pointer is invalid; entries skipped\n");
    } else {
        // Scan pointer slots through the sane cap so the local-membership result
        // covers the candidate list, while position reads/logging remain limited.
        for (std::uint32_t i = 0; i < cappedCount; ++i) {
            const bool logEntry = i < inspectCount;
            const std::uint64_t entryOffset = std::uint64_t{i} * sizeof(std::uint64_t);
            std::uint64_t entryAddress{}, entity{};
            if (!AddAddress(data, entryOffset, entryAddress) || !UserAddress(entryAddress)
                || !reader.Read(entryAddress, entity)) {
                if (logEntry)
                    DiagnosticLog("[entity-diagnostic] [%u] entity=<read failed>; skipped\n", i);
                continue;
            }
            if (!UserAddress(entity)) {
                if (logEntry)
                    DiagnosticLog("[entity-diagnostic] [%u] entity=0x%llX invalid; skipped\n",
                        i, static_cast<unsigned long long>(entity));
                continue;
            }
            if (haveLocal && entity == localCharacter) localInList = true;
            if (!logEntry) continue;

            std::uint64_t positionAddress{};
            Vector3 position{};
            if (!AddAddress(entity, Config::EntityWorldPosition, positionAddress)
                || !UserAddress(positionAddress)
                || !reader.Read(positionAddress, position)) {
                DiagnosticLog("[entity-diagnostic] [%u] entity=0x%llX position=<read failed>; skipped\n",
                    i, static_cast<unsigned long long>(entity));
                continue;
            }
            if (!Finite(position)) {
                DiagnosticLog("[entity-diagnostic] [%u] entity=0x%llX position=<non-finite>; skipped\n",
                    i, static_cast<unsigned long long>(entity));
                continue;
            }
            DiagnosticLog("[entity-diagnostic] [%u] entity=0x%llX position=(%.6f, %.6f, %.6f)\n",
                i, static_cast<unsigned long long>(entity),
                static_cast<double>(position.x), static_cast<double>(position.y),
                static_cast<double>(position.z));
        }
    }

    DiagnosticLog("[entity-diagnostic] local_in_capped_list=%s%s\n",
        localInList ? "yes" : "no", haveLocal ? "" : " (local unresolved)");
    DiagnosticLog("[entity-diagnostic] end\n");
}

struct ClassifiedEntity {
    std::uint64_t pointer{};
    std::string className;
    Vector3 position{};
    bool positionValid = false;
};

std::string HexBytes(const std::array<unsigned char, 96>& bytes, std::size_t count) {
    static constexpr char Digits[] = "0123456789ABCDEF";
    std::string result;
    count = (std::min)(count, bytes.size());
    result.reserve(count * 3);
    for (std::size_t i = 0; i < count; ++i) {
        if (i) result.push_back(' ');
        result.push_back(Digits[bytes[i] >> 4]);
        result.push_back(Digits[bytes[i] & 0x0F]);
    }
    return result;
}

bool PrintableAscii(unsigned char value) {
    return value >= 0x20 && value <= 0x7E;
}

template<class Reader>
bool ReadClassName(const Reader& reader, std::uint64_t entity, std::string& className,
                   bool& unreadableText, std::string& rawHex) {
    className.clear();
    unreadableText = false;
    rawHex.clear();

    std::uint64_t firstField{}, first{}, secondField{}, second{}, stringField{}, stringAddress{};
    if (!AddAddress(entity, 0xE8, firstField) || !UserAddress(firstField)
        || !reader.Read(firstField, first) || !UserAddress(first)
        || !AddAddress(first, 0x08, secondField) || !UserAddress(secondField)
        || !reader.Read(secondField, second) || !UserAddress(second)
        || !AddAddress(second, 0x10, stringField) || !UserAddress(stringField)
        || !reader.Read(stringField, stringAddress) || !UserAddress(stringAddress))
        return false;

    std::array<unsigned char, 96> bytes{};
    if (!reader.Read(stringAddress, bytes)) return false;

    std::size_t wideLength = 0;
    while ((wideLength + 1) * 2 <= bytes.size()) {
        const auto low = bytes[wideLength * 2];
        const auto high = bytes[wideLength * 2 + 1];
        if (low == 0 && high == 0) break;
        if (high != 0 || !PrintableAscii(low)) { wideLength = 0; break; }
        ++wideLength;
    }
    if (wideLength >= 2) {
        className.reserve(wideLength);
        for (std::size_t i = 0; i < wideLength; ++i)
            className.push_back(static_cast<char>(bytes[i * 2]));
        return true;
    }

    std::size_t narrowLength = 0;
    while (narrowLength < bytes.size() && bytes[narrowLength] != 0
        && PrintableAscii(bytes[narrowLength])) ++narrowLength;
    if (narrowLength >= 2
        && (narrowLength == bytes.size() || bytes[narrowLength] == 0)) {
        className.assign(reinterpret_cast<const char*>(bytes.data()), narrowLength);
        return true;
    }

    unreadableText = true;
    rawHex = HexBytes(bytes, 32);
    className = "<unreadable>";
    return true;
}

bool CharacterLike(const std::string& className) {
    std::string lower = className;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) {
        return static_cast<char>(value >= 'A' && value <= 'Z' ? value - 'A' + 'a' : value);
    });
    if (lower.find("camera") != std::string::npos
        || lower.find("controller") != std::string::npos) return false;
    constexpr const char* Keywords[] = {
        "character", "soldier", "infantry", "player", "human", "unit", "agent"
    };
    for (const char* keyword : Keywords)
        if (lower.find(keyword) != std::string::npos) return true;
    return false;
}

template<class Reader>
void LogEntityClassDiagnostic(const Reader& reader, std::uint64_t moduleBase) {
    DiagnosticLog("[class-diagnostic] begin\n");

    std::uint64_t rootSlot{}, game{}, worldField{}, world{};
    if (!Config::RootConfigured
        || !AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game) || !UserAddress(game)
        || !AddAddress(game, Config::World, worldField) || !UserAddress(worldField)
        || !reader.Read(worldField, world) || !UserAddress(world)) {
        DiagnosticLog("[class-diagnostic] game/world resolution failed\n");
        return;
    }

    std::uint64_t localField{}, localCharacter{};
    WeakSnapshot localSnapshot{};
    const bool haveLocal = AddAddress(world, Config::WorldLocalCharacter, localField)
        && ReadWeakSnapshot(reader, localField, localSnapshot);
    if (haveLocal) localCharacter = localSnapshot.object;

    std::uint64_t dataField{}, countField{}, data{};
    std::uint32_t rawCount{};
    if (!AddAddress(world, Config::EntityArrayData, dataField) || !UserAddress(dataField)
        || !AddAddress(world, Config::EntityArrayCount, countField) || !UserAddress(countField)
        || !reader.Read(dataField, data) || !UserAddress(data)
        || !reader.Read(countField, rawCount)) {
        DiagnosticLog("[class-diagnostic] entity array resolution failed\n");
        return;
    }

    const std::uint32_t scanCount = (std::min)(rawCount, Config::FullEntityScanLimit);
    DiagnosticLog("[class-diagnostic] game=0x%llX world=0x%llX data=0x%llX raw_count=%u scan_count=%u%s\n",
        static_cast<unsigned long long>(game), static_cast<unsigned long long>(world),
        static_cast<unsigned long long>(data), rawCount, scanCount,
        rawCount > scanCount ? " TRUNCATED_AT_SAFETY_LIMIT" : "");
    DiagnosticLog("[class-diagnostic] local=0x%llX%s\n",
        static_cast<unsigned long long>(localCharacter), haveLocal ? "" : " (unresolved)");

    std::vector<ClassifiedEntity> entities;
    entities.reserve(scanCount);
    std::unordered_map<std::uint64_t, std::size_t> distinct;
    distinct.reserve(scanCount);
    std::vector<std::uint32_t> localIndices;
    for (std::uint32_t i = 0; i < scanCount; ++i) {
        std::uint64_t entryAddress{}, entity{};
        const std::uint64_t offset = std::uint64_t{i} * sizeof(std::uint64_t);
        if (!AddAddress(data, offset, entryAddress) || !UserAddress(entryAddress)
            || !reader.Read(entryAddress, entity) || !UserAddress(entity)) continue;
        if (haveLocal && entity == localCharacter) localIndices.push_back(i);
        if (distinct.emplace(entity, entities.size()).second)
            entities.push_back(ClassifiedEntity{entity});
    }
    DiagnosticLog("[class-diagnostic] distinct_valid_entities=%zu\n", entities.size());

    std::unordered_map<std::string, std::size_t> histogram;
    std::size_t rawExampleCount = 0;
    for (auto& entity : entities) {
        bool unreadableText = false;
        std::string rawHex;
        if (!ReadClassName(reader, entity.pointer, entity.className, unreadableText, rawHex))
            entity.className = "no class";
        else if (unreadableText && rawExampleCount < 5) {
            DiagnosticLog("[class-diagnostic] unreadable_class entity=0x%llX raw=%s\n",
                static_cast<unsigned long long>(entity.pointer), rawHex.c_str());
            ++rawExampleCount;
        }
        ++histogram[entity.className];

        std::uint64_t positionAddress{};
        entity.positionValid = AddAddress(entity.pointer, Config::EntityWorldPosition, positionAddress)
            && UserAddress(positionAddress) && reader.Read(positionAddress, entity.position)
            && Finite(entity.position);
    }

    std::vector<std::pair<std::string, std::size_t>> sortedHistogram(
        histogram.begin(), histogram.end());
    std::sort(sortedHistogram.begin(), sortedHistogram.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second > right.second;
        return left.first < right.first;
    });
    DiagnosticLog("[class-diagnostic] histogram_begin buckets=%zu\n", sortedHistogram.size());
    for (const auto& bucket : sortedHistogram)
        DiagnosticLog("[class-diagnostic] histogram count=%zu class=%s\n",
            bucket.second, bucket.first.c_str());
    DiagnosticLog("[class-diagnostic] histogram_end\n");

    if (!haveLocal) {
        DiagnosticLog("[class-diagnostic] local_indices=<local unresolved>\n");
    } else if (localIndices.empty()) {
        DiagnosticLog("[class-diagnostic] local_indices=<not found>\n");
    } else {
        DiagnosticLog("[class-diagnostic] local_indices=");
        for (std::size_t i = 0; i < localIndices.size(); ++i)
            DiagnosticLog("%s%u", i ? "," : "", localIndices[i]);
        DiagnosticLog("\n");
    }

    std::string localClass = "no class";
    if (haveLocal) {
        bool unreadableText = false;
        std::string rawHex;
        if (ReadClassName(reader, localCharacter, localClass, unreadableText, rawHex)
            && unreadableText)
            DiagnosticLog("[class-diagnostic] local_class_raw=%s\n", rawHex.c_str());
    }
    DiagnosticLog("[class-diagnostic] local_class=%s\n", localClass.c_str());

    Vector3 localPosition{};
    std::uint64_t localPositionAddress{};
    const bool haveLocalPosition = haveLocal
        && AddAddress(localCharacter, Config::EntityWorldPosition, localPositionAddress)
        && UserAddress(localPositionAddress) && reader.Read(localPositionAddress, localPosition)
        && Finite(localPosition);
    if (haveLocalPosition)
        DiagnosticLog("[class-diagnostic] local_position=(%.6f, %.6f, %.6f)\n",
            static_cast<double>(localPosition.x), static_cast<double>(localPosition.y),
            static_cast<double>(localPosition.z));
    else
        DiagnosticLog("[class-diagnostic] local_position=<unavailable>\n");

    std::vector<std::string> selectedClasses;
    for (const auto& bucket : sortedHistogram) {
        if (CharacterLike(bucket.first)) selectedClasses.push_back(bucket.first);
        if (selectedClasses.size() == 3) break;
    }
    std::vector<const ClassifiedEntity*> characterEntities;
    for (const auto& entity : entities) {
        if (std::find(selectedClasses.begin(), selectedClasses.end(), entity.className)
            != selectedClasses.end()) characterEntities.push_back(&entity);
    }
    auto distanceFromLocal = [&](const ClassifiedEntity* entity) {
        if (!haveLocalPosition || !entity->positionValid)
            return (std::numeric_limits<double>::infinity)();
        const double x = double(entity->position.x) - localPosition.x;
        const double y = double(entity->position.y) - localPosition.y;
        const double z = double(entity->position.z) - localPosition.z;
        return std::sqrt(x*x + y*y + z*z);
    };
    std::sort(characterEntities.begin(), characterEntities.end(), [&](const auto* left, const auto* right) {
        return distanceFromLocal(left) < distanceFromLocal(right);
    });
    DiagnosticLog("[class-diagnostic] character_like_classes=");
    if (selectedClasses.empty()) DiagnosticLog("<none>");
    for (std::size_t i = 0; i < selectedClasses.size(); ++i)
        DiagnosticLog("%s%s", i ? "," : "", selectedClasses[i].c_str());
    DiagnosticLog(" entities=%zu\n", characterEntities.size());

    const std::size_t reportCount = (std::min)(characterEntities.size(), std::size_t{20});
    for (std::size_t i = 0; i < reportCount; ++i) {
        const auto* entity = characterEntities[i];
        const double distance = distanceFromLocal(entity);
        if (entity->positionValid && std::isfinite(distance)) {
            DiagnosticLog("[class-diagnostic] character entity=0x%llX class=%s position=(%.6f, %.6f, %.6f) distance=%.6f\n",
                static_cast<unsigned long long>(entity->pointer), entity->className.c_str(),
                static_cast<double>(entity->position.x), static_cast<double>(entity->position.y),
                static_cast<double>(entity->position.z), distance);
        } else {
            DiagnosticLog("[class-diagnostic] character entity=0x%llX class=%s position=<unavailable> distance=<unavailable>\n",
                static_cast<unsigned long long>(entity->pointer), entity->className.c_str());
        }
    }
    if (characterEntities.size() > reportCount)
        DiagnosticLog("[class-diagnostic] character_output_truncated=%zu\n",
            characterEntities.size() - reportCount);
    DiagnosticLog("[class-diagnostic] end\n");
}

std::string DecodeNarrowText(const std::array<unsigned char, 96>& bytes) {
    std::size_t length = 0;
    while (length < bytes.size() && bytes[length] != 0 && PrintableAscii(bytes[length]))
        ++length;
    if (length < 2 || length == bytes.size() || bytes[length] != 0) return {};
    return std::string(reinterpret_cast<const char*>(bytes.data()), length);
}

std::string DecodeWideText(const std::array<unsigned char, 96>& bytes) {
    std::string result;
    for (std::size_t offset = 0; offset + 1 < bytes.size(); offset += 2) {
        const unsigned char low = bytes[offset], high = bytes[offset + 1];
        if (low == 0 && high == 0) return result.size() >= 2 ? result : std::string{};
        if (high != 0 || !PrintableAscii(low)) return {};
        result.push_back(static_cast<char>(low));
    }
    return {};
}

template<class Reader>
void LogFactionDiagnostic(const Reader& reader, std::uint64_t moduleBase) {
    DiagnosticLog("[faction-diagnostic] begin\n");
    std::uint64_t rootSlot{}, game{}, worldField{}, world{}, dataField{}, countField{}, data{};
    std::uint64_t localField{}, localCharacter{};
    WeakSnapshot local{};
    std::uint32_t count{};
    if (!Config::RootConfigured
        || !AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game) || !UserAddress(game)
        || !AddAddress(game, Config::World, worldField) || !UserAddress(worldField)
        || !reader.Read(worldField, world) || !UserAddress(world)
        || !AddAddress(world, Config::EntityArrayData, dataField) || !UserAddress(dataField)
        || !AddAddress(world, Config::EntityArrayCount, countField) || !UserAddress(countField)
        || !reader.Read(dataField, data) || !UserAddress(data)
        || !reader.Read(countField, count)
        || !AddAddress(world, Config::WorldLocalCharacter, localField)
        || !ReadWeakSnapshot(reader, localField, local)) {
        DiagnosticLog("[faction-diagnostic] world/list/local resolution failed\n");
        return;
    }
    localCharacter = local.object;
    if (count > Config::FullEntityScanLimit) {
        DiagnosticLog("[faction-diagnostic] count=%u exceeds safety_limit=%u\n",
            count, Config::FullEntityScanLimit);
        return;
    }

    std::vector<std::uint64_t> pointers;
    pointers.reserve(count);
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(count);
    std::uint32_t index = 0;
    while (index + Config::EntityPointerBatchSize <= count) {
        std::array<std::uint64_t, Config::EntityPointerBatchSize> batch{};
        std::uint64_t batchAddress{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), batchAddress)
            || !UserAddress(batchAddress) || !reader.Read(batchAddress, batch)) {
            DiagnosticLog("[faction-diagnostic] pointer batch read failed index=%u\n", index);
            return;
        }
        for (const auto entity : batch)
            if (UserAddress(entity) && seen.insert(entity).second) pointers.push_back(entity);
        index += Config::EntityPointerBatchSize;
    }
    for (; index < count; ++index) {
        std::uint64_t entryAddress{}, entity{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), entryAddress)
            || !UserAddress(entryAddress) || !reader.Read(entryAddress, entity)) {
            DiagnosticLog("[faction-diagnostic] pointer read failed index=%u\n", index);
            return;
        }
        if (UserAddress(entity) && seen.insert(entity).second) pointers.push_back(entity);
    }

    std::vector<std::uint64_t> characters;
    for (const auto entity : pointers) {
        bool unreadableText = false;
        std::string className, rawHex;
        if (ReadClassName(reader, entity, className, unreadableText, rawHex)
            && !unreadableText && className == Config::CharacterClassName)
            characters.push_back(entity);
    }
    if (std::find(characters.begin(), characters.end(), localCharacter) == characters.end()) {
        bool unreadableText = false;
        std::string className, rawHex;
        if (ReadClassName(reader, localCharacter, className, unreadableText, rawHex)
            && !unreadableText && className == Config::CharacterClassName)
            characters.push_back(localCharacter);
    }
    DiagnosticLog("[faction-diagnostic] array_count=%u distinct=%zu characters=%zu local=0x%llX\n",
        count, pointers.size(), characters.size(),
        static_cast<unsigned long long>(localCharacter));

    for (const auto entity : characters) {
        std::uint64_t factionField{}, controlBlock{}, interior{}, factionObject{};
        if (!AddAddress(entity, 0x178, factionField) || !UserAddress(factionField)
            || !reader.Read(factionField, controlBlock)) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX faction field read failed\n",
                static_cast<unsigned long long>(entity));
            continue;
        }
        if (!controlBlock) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX%s no faction\n",
                static_cast<unsigned long long>(entity),
                entity == localCharacter ? " LOCAL" : "");
            continue;
        }
        if (!UserAddress(controlBlock) || !reader.Read(controlBlock, interior)
            || interior <= 0x10 || !UserAddress(interior)) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX%s invalid faction weak link control=0x%llX interior=0x%llX\n",
                static_cast<unsigned long long>(entity), entity == localCharacter ? " LOCAL" : "",
                static_cast<unsigned long long>(controlBlock),
                static_cast<unsigned long long>(interior));
            continue;
        }
        factionObject = interior - 0x10;
        if (!UserAddress(factionObject)) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX%s invalid faction object=0x%llX\n",
                static_cast<unsigned long long>(entity), entity == localCharacter ? " LOCAL" : "",
                static_cast<unsigned long long>(factionObject));
            continue;
        }

        std::array<unsigned char, 0x40> dump{};
        if (!reader.Read(factionObject, dump)) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX%s faction=0x%llX dump failed\n",
                static_cast<unsigned long long>(entity), entity == localCharacter ? " LOCAL" : "",
                static_cast<unsigned long long>(factionObject));
            continue;
        }

        struct CandidateText {
            std::size_t offset{};
            std::uint64_t pointer{};
            std::string narrow, wide;
        };
        std::vector<CandidateText> candidates;
        for (std::size_t offset = 0; offset + sizeof(std::uint64_t) <= dump.size();
             offset += sizeof(std::uint64_t)) {
            std::uint64_t pointer{};
            std::memcpy(&pointer, dump.data() + offset, sizeof(pointer));
            if (!UserAddress(pointer)) continue;
            std::array<unsigned char, 96> textBytes{};
            if (!reader.Read(pointer, textBytes)) continue;
            std::string narrow = DecodeNarrowText(textBytes);
            std::string wide = DecodeWideText(textBytes);
            if (!narrow.empty() || !wide.empty())
                candidates.push_back(CandidateText{offset, pointer, std::move(narrow), std::move(wide)});
        }

        std::uint64_t controlAgain{}, interiorAgain{};
        if (!reader.Read(factionField, controlAgain) || controlAgain != controlBlock
            || !reader.Read(controlBlock, interiorAgain) || interiorAgain != interior) {
            DiagnosticLog("[faction-diagnostic] entity=0x%llX%s faction changed during read; skipped\n",
                static_cast<unsigned long long>(entity), entity == localCharacter ? " LOCAL" : "");
            continue;
        }

        DiagnosticLog("[faction-diagnostic] entity=0x%llX%s control=0x%llX interior=0x%llX faction=0x%llX\n",
            static_cast<unsigned long long>(entity), entity == localCharacter ? " LOCAL" : "",
            static_cast<unsigned long long>(controlBlock),
            static_cast<unsigned long long>(interior),
            static_cast<unsigned long long>(factionObject));
        for (std::size_t offset = 0; offset < dump.size(); offset += 16) {
            char line[16 * 3 + 1]{};
            for (std::size_t byte = 0; byte < 16; ++byte)
                sprintf_s(line + byte * 3, sizeof(line) - byte * 3, "%02X%s",
                    dump[offset + byte], byte == 15 ? "" : " ");
            DiagnosticLog("[faction-diagnostic]   +0x%02zX  %s\n", offset, line);
        }
        if (candidates.empty())
            DiagnosticLog("[faction-diagnostic]   candidate_strings=<none>\n");
        for (const auto& candidate : candidates) {
            DiagnosticLog("[faction-diagnostic]   pointer_field=+0x%02zX pointer=0x%llX ascii=%s utf16=%s\n",
                candidate.offset, static_cast<unsigned long long>(candidate.pointer),
                candidate.narrow.empty() ? "<none>" : candidate.narrow.c_str(),
                candidate.wide.empty() ? "<none>" : candidate.wide.c_str());
        }
    }
    DiagnosticLog("[faction-diagnostic] end\n");
}

template<class Reader>
bool ReadTextAt(const Reader& reader, std::uint64_t address, std::string& text,
                std::string& encoding, std::string& rawHex) {
    text.clear();
    encoding.clear();
    rawHex.clear();
    if (!UserAddress(address)) return false;
    std::array<unsigned char, 96> bytes{};
    if (!reader.Read(address, bytes)) return false;
    rawHex = HexBytes(bytes, 32);
    text = DecodeNarrowText(bytes);
    if (!text.empty()) { encoding = "8-bit"; return true; }
    text = DecodeWideText(bytes);
    if (!text.empty()) { encoding = "UTF-16"; return true; }
    return false;
}

#pragma pack(push, 1)
struct HealthPair {
    float maximum;
    float current;
};
#pragma pack(pop)
static_assert(offsetof(HealthPair, maximum) == 0x00);
static_assert(offsetof(HealthPair, current) == 0x04);
static_assert(sizeof(HealthPair) == 0x08);

template<class Reader>
bool ReadHealthFloats(const Reader& reader, std::uint64_t object,
                      float& current, float& maximum) {
    std::uint64_t blockAddress{};
    HealthPair block{};
    if (!UserAddress(object)
        || !AddAddress(object, 0x38, blockAddress) || !UserAddress(blockAddress)
        || !reader.Read(blockAddress, block)
        || !std::isfinite(block.maximum) || !std::isfinite(block.current)
        || block.maximum <= 0.0f || block.maximum > 1000000.0f
        || block.current < -1000000.0f || block.current > block.maximum * 10.0f)
        return false;
    current = block.current;
    maximum = block.maximum;
    return true;
}

struct CharacterMetadata {
    std::uint64_t entity{};
    std::uint64_t entityLink{};
    std::string faction = "no faction";
    float hp = 0, maxHp = 0;
    bool healthValid = false;
    bool dead = false;
    std::string playerName = "AI";
};

struct PlayerMetadata {
    std::uint64_t object{};
    std::uint64_t entityLink{};
    std::string name = "<unreadable>";
};

template<class Reader>
void LogCharacterMetadataDiagnostic(const Reader& reader, std::uint64_t moduleBase) {
    DiagnosticLog("[metadata-diagnostic] begin\n");
    std::uint64_t rootSlot{}, game{}, worldField{}, world{}, dataField{}, countField{}, data{};
    std::uint32_t count{};
    if (!Config::RootConfigured
        || !AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game) || !UserAddress(game)
        || !AddAddress(game, Config::World, worldField) || !UserAddress(worldField)
        || !reader.Read(worldField, world) || !UserAddress(world)
        || !AddAddress(world, Config::EntityArrayData, dataField) || !UserAddress(dataField)
        || !AddAddress(world, Config::EntityArrayCount, countField) || !UserAddress(countField)
        || !reader.Read(dataField, data) || !UserAddress(data)
        || !reader.Read(countField, count)) {
        DiagnosticLog("[metadata-diagnostic] world/list resolution failed\n");
        return;
    }
    if (count > Config::FullEntityScanLimit) {
        DiagnosticLog("[metadata-diagnostic] world count=%u exceeds safety_limit=%u\n",
            count, Config::FullEntityScanLimit);
        return;
    }

    std::vector<std::uint64_t> pointers;
    pointers.reserve(count);
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(count);
    std::uint32_t index = 0;
    while (index + Config::EntityPointerBatchSize <= count) {
        std::array<std::uint64_t, Config::EntityPointerBatchSize> batch{};
        std::uint64_t batchAddress{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), batchAddress)
            || !UserAddress(batchAddress) || !reader.Read(batchAddress, batch)) {
            DiagnosticLog("[metadata-diagnostic] world pointer batch failed index=%u\n", index);
            return;
        }
        for (const auto entity : batch)
            if (UserAddress(entity) && seen.insert(entity).second) pointers.push_back(entity);
        index += Config::EntityPointerBatchSize;
    }
    for (; index < count; ++index) {
        std::uint64_t entryAddress{}, entity{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), entryAddress)
            || !UserAddress(entryAddress) || !reader.Read(entryAddress, entity)) {
            DiagnosticLog("[metadata-diagnostic] world pointer failed index=%u\n", index);
            return;
        }
        if (UserAddress(entity) && seen.insert(entity).second) pointers.push_back(entity);
    }

    std::vector<CharacterMetadata> characters;
    for (const auto entity : pointers) {
        bool unreadableText = false;
        std::string className, rawHex;
        if (!ReadClassName(reader, entity, className, unreadableText, rawHex)
            || unreadableText || className != Config::CharacterClassName) continue;
        CharacterMetadata metadata{};
        metadata.entity = entity;
        std::uint64_t entityLinkField{};
        if (AddAddress(entity, 0x20, entityLinkField) && UserAddress(entityLinkField))
            reader.Read(entityLinkField, metadata.entityLink);
        characters.push_back(std::move(metadata));
    }
    DiagnosticLog("[metadata-diagnostic] world_count=%u distinct=%zu characters=%zu\n",
        count, pointers.size(), characters.size());

    for (auto& character : characters) {
        std::uint64_t factionField{}, fw{}, fdata{}, fdataField{}, factionTextAddress{};
        if (AddAddress(character.entity, 0x178, factionField) && UserAddress(factionField)
            && reader.Read(factionField, fw) && UserAddress(fw)
            && AddAddress(fw, 0x00, fdataField) && UserAddress(fdataField)
            && reader.Read(fdataField, fdata) && UserAddress(fdata)
            && AddAddress(fdata, 0x60, factionTextAddress) && UserAddress(factionTextAddress)) {
            std::string encoding, rawHex, factionText;
            bool textRead = ReadTextAt(
                reader, factionTextAddress, factionText, encoding, rawHex);
            if (!textRead) {
                std::uint64_t factionTextPointer{};
                std::string pointedRaw;
                if (reader.Read(factionTextAddress, factionTextPointer)
                    && UserAddress(factionTextPointer)
                    && ReadTextAt(reader, factionTextPointer, factionText, encoding, pointedRaw)) {
                    textRead = true;
                    encoding = "pointer/" + encoding;
                }
            }
            DiagnosticLog("[metadata-diagnostic] faction entity=0x%llX fw=0x%llX fdata=0x%llX raw=%s\n",
                static_cast<unsigned long long>(character.entity),
                static_cast<unsigned long long>(fw), static_cast<unsigned long long>(fdata),
                rawHex.empty() ? "<read failed>" : rawHex.c_str());
            if (textRead) character.faction = factionText;
            DiagnosticLog("[metadata-diagnostic] faction entity=0x%llX value=%s encoding=%s\n",
                static_cast<unsigned long long>(character.entity), character.faction.c_str(),
                textRead ? encoding.c_str() : "<none>");
        } else {
            DiagnosticLog("[metadata-diagnostic] faction entity=0x%llX no faction\n",
                static_cast<unsigned long long>(character.entity));
        }

        std::uint64_t damageField{}, dm{}, healthField{}, hz{};
        std::array<unsigned char, 0x10> healthHeader{};
        std::string healthPath = "<unresolved>";
        if (AddAddress(character.entity, 0x130, damageField) && UserAddress(damageField)
            && reader.Read(damageField, dm) && UserAddress(dm)
            && AddAddress(dm, 0x60, healthField) && UserAddress(healthField)
            && reader.Read(healthField, healthHeader)) {
            std::memcpy(&hz, healthHeader.data(), sizeof(hz));
            DiagnosticLog("[metadata-diagnostic] health entity=0x%llX dm=0x%llX direct=0x%llX\n",
                static_cast<unsigned long long>(character.entity),
                static_cast<unsigned long long>(dm),
                static_cast<unsigned long long>(hz));
            char headerLine[0x10 * 3 + 1]{};
            for (std::size_t byte = 0; byte < healthHeader.size(); ++byte)
                sprintf_s(headerLine + byte * 3, sizeof(headerLine) - byte * 3, "%02X%s",
                    healthHeader[byte], byte + 1 == healthHeader.size() ? "" : " ");
            DiagnosticLog("[metadata-diagnostic] health_header entity=0x%llX raw=%s\n",
                static_cast<unsigned long long>(character.entity), headerLine);

            if (ReadHealthFloats(reader, hz, character.hp, character.maxHp)) {
                character.healthValid = true;
                healthPath = "direct";
            } else if (UserAddress(hz)) {
                for (std::uint64_t offset : {std::uint64_t{0}, std::uint64_t{8}}) {
                    std::uint64_t candidateField{}, candidate{};
                    if (AddAddress(hz, offset, candidateField) && UserAddress(candidateField)
                        && reader.Read(candidateField, candidate) && UserAddress(candidate)
                        && ReadHealthFloats(reader, candidate, character.hp, character.maxHp)) {
                        character.healthValid = true;
                        healthPath = offset ? "holder+0x08" : "holder+0x00";
                        break;
                    }
                }
            }
        }
        if (character.healthValid) character.dead = character.hp <= 0.0f;
        DiagnosticLog("[metadata-diagnostic] health entity=0x%llX hp=%s path=%s dead=%s\n",
            static_cast<unsigned long long>(character.entity),
            character.healthValid
                ? (std::to_string(character.hp) + "/" + std::to_string(character.maxHp)).c_str()
                : "<unavailable>", healthPath.c_str(),
            character.healthValid ? (character.dead ? "yes" : "no") : "<unknown>");
    }

    std::vector<PlayerMetadata> players;
    std::uint64_t managerField{}, manager{}, listField{}, list{};
    std::uint64_t playerCountField{};
    std::uint32_t playerCount{};
    constexpr std::uint32_t MaxPlayerCount = 512;
    if (AddAddress(game, 0x2D8, managerField) && UserAddress(managerField)
        && reader.Read(managerField, manager) && UserAddress(manager)
        && AddAddress(manager, 0x18, listField) && UserAddress(listField)
        && reader.Read(listField, list) && UserAddress(list)
        && AddAddress(manager, 0x24, playerCountField) && UserAddress(playerCountField)
        && reader.Read(playerCountField, playerCount) && playerCount <= MaxPlayerCount) {
        players.reserve(playerCount);
        for (std::uint32_t i = 0; i < playerCount; ++i) {
            std::uint64_t slot{}, playerObject{};
            if (!AddAddress(list, std::uint64_t{i} * sizeof(std::uint64_t), slot)
                || !UserAddress(slot) || !reader.Read(slot, playerObject)
                || !UserAddress(playerObject)) continue;
            PlayerMetadata player{};
            player.object = playerObject;

            std::uint64_t nameAddress{};
            std::string encoding, rawHex;
            if (AddAddress(playerObject, 0x18, nameAddress) && UserAddress(nameAddress)) {
                if (!ReadTextAt(reader, nameAddress, player.name, encoding, rawHex)) {
                    std::uint64_t namePointer{};
                    if (!reader.Read(nameAddress, namePointer) || !UserAddress(namePointer)
                        || !ReadTextAt(reader, namePointer, player.name, encoding, rawHex))
                        player.name = "<unreadable>";
                }
                if (player.name == "<unreadable>")
                    DiagnosticLog("[metadata-diagnostic] player_name_raw index=%u object=0x%llX raw=%s\n",
                        i, static_cast<unsigned long long>(playerObject),
                        rawHex.empty() ? "<read failed>" : rawHex.c_str());
            }

            std::uint64_t firstField{}, first{}, secondField{}, second{}, linkField{};
            if (AddAddress(playerObject, 0x48, firstField) && UserAddress(firstField)
                && reader.Read(firstField, first) && UserAddress(first)
                && AddAddress(first, 0x08, secondField) && UserAddress(secondField)
                && reader.Read(secondField, second) && UserAddress(second)
                && AddAddress(second, 0x10, linkField) && UserAddress(linkField))
                reader.Read(linkField, player.entityLink);
            DiagnosticLog("[metadata-diagnostic] player_link index=%u object=0x%llX hop1=0x%llX hop2=0x%llX link=0x%llX\n",
                i, static_cast<unsigned long long>(playerObject),
                static_cast<unsigned long long>(first), static_cast<unsigned long long>(second),
                static_cast<unsigned long long>(player.entityLink));
            if (!UserAddress(player.entityLink) && UserAddress(first)) {
                player.entityLink = first;
                DiagnosticLog("[metadata-diagnostic] player_link index=%u using_direct_+0x48_fallback=0x%llX\n",
                    i, static_cast<unsigned long long>(player.entityLink));
            }
            players.push_back(std::move(player));
        }
    } else {
        DiagnosticLog("[metadata-diagnostic] player manager/list unavailable or count invalid=%u\n",
            playerCount);
    }

    DiagnosticLog("[metadata-diagnostic] player_list count=%zu\n", players.size());
    for (const auto& player : players) {
        DiagnosticLog("[metadata-diagnostic] player object=0x%llX name=%s entity_link=0x%llX maps_to=",
            static_cast<unsigned long long>(player.object), player.name.c_str(),
            static_cast<unsigned long long>(player.entityLink));
        bool matched = false;
        for (auto& character : characters) {
            if (UserAddress(player.entityLink) && player.entityLink == character.entityLink) {
                DiagnosticLog("%s0x%llX", matched ? "," : "",
                    static_cast<unsigned long long>(character.entity));
                if (character.playerName == "AI") character.playerName = player.name;
                matched = true;
            }
        }
        if (!matched) DiagnosticLog("<none>");
        DiagnosticLog("\n");
    }

    DiagnosticLog("[metadata-diagnostic] summary_begin\n");
    DiagnosticLog("[metadata-diagnostic] entity | faction | HP/maxHP | dead | player\n");
    for (const auto& character : characters) {
        DiagnosticLog("[metadata-diagnostic] entity_link entity=0x%llX link=0x%llX\n",
            static_cast<unsigned long long>(character.entity),
            static_cast<unsigned long long>(character.entityLink));
        if (character.healthValid) {
            DiagnosticLog("[metadata-diagnostic] 0x%llX | %s | %.3f/%.3f | %s | %s\n",
                static_cast<unsigned long long>(character.entity), character.faction.c_str(),
                static_cast<double>(character.hp), static_cast<double>(character.maxHp),
                character.dead ? "yes" : "no", character.playerName.c_str());
        } else {
            DiagnosticLog("[metadata-diagnostic] 0x%llX | %s | unavailable | unknown | %s\n",
                static_cast<unsigned long long>(character.entity), character.faction.c_str(),
                character.playerName.c_str());
        }
    }
    DiagnosticLog("[metadata-diagnostic] summary_end\n");
    DiagnosticLog("[metadata-diagnostic] end\n");
}

bool NonZero(Vector3 value) {
    return value.x != 0.0f || value.y != 0.0f || value.z != 0.0f;
}

double Distance(Vector3 left, Vector3 right) {
    const double x = double(left.x) - right.x;
    const double y = double(left.y) - right.y;
    const double z = double(left.z) - right.z;
    return std::sqrt(x*x + y*y + z*z);
}

struct CharacterVisualMetadata {
    std::string faction = "Unknown";
    float hp = 0, maxHp = 0;
    bool healthValid = false;
    bool dead = false;
    std::string playerName;
};

bool FactionEnabled(const std::string& faction) {
    if (faction == "US") return Config::ShowUS;
    if (faction == "USSR") return Config::ShowUSSR;
    if (faction == "FIA") return Config::ShowFIA;
    if (faction == "CIV") return Config::ShowCIV;
    return true;
}

bool CharacterMetadataEnabled(const CharacterVisualMetadata* metadata) {
    if (!metadata) return Config::ShowAlive;
    if (!FactionEnabled(metadata->faction)) return false;
    if (!metadata->healthValid) return Config::ShowAlive;
    return metadata->dead ? Config::ShowDead : Config::ShowAlive;
}

struct CharacterScanState {
    std::vector<std::uint64_t> entities;
    std::unordered_map<std::uint64_t, std::string> classCache;
    std::unordered_map<std::uint64_t, CharacterVisualMetadata> metadataCache;
    std::uint32_t lastCount = 0;
    bool hasAcceptedCount = false;
};

template<class Reader>
std::string ReadFactionForOverlay(const Reader& reader, std::uint64_t entity) {
    std::uint64_t field{}, fw{}, fdataField{}, fdata{}, stringField{}, stringPointer{};
    if (!AddAddress(entity, 0x178, field) || !UserAddress(field)
        || !reader.Read(field, fw) || !UserAddress(fw)
        || !AddAddress(fw, 0x00, fdataField) || !UserAddress(fdataField)
        || !reader.Read(fdataField, fdata) || !UserAddress(fdata)
        || !AddAddress(fdata, 0x60, stringField) || !UserAddress(stringField)
        || !reader.Read(stringField, stringPointer) || !UserAddress(stringPointer))
        return "Unknown";
    std::array<unsigned char, 96> bytes{};
    if (!reader.Read(stringPointer, bytes)) return "Unknown";
    const std::string faction = DecodeNarrowText(bytes);
    return faction.empty() ? "Unknown" : faction;
}

template<class Reader>
void ReadHealthForOverlay(const Reader& reader, std::uint64_t entity,
                          CharacterVisualMetadata& metadata) {
    metadata.healthValid = false;
    metadata.dead = false;
    metadata.hp = 0;
    metadata.maxHp = 0;
    std::uint64_t damageField{}, damage{}, holderField{}, holder{};
    std::uint64_t hitzoneField{}, hitzone{};
    if (!AddAddress(entity, 0x130, damageField) || !UserAddress(damageField)
        || !reader.Read(damageField, damage) || !UserAddress(damage)
        || !AddAddress(damage, 0x60, holderField) || !UserAddress(holderField)
        || !reader.Read(holderField, holder) || !UserAddress(holder)
        || !AddAddress(holder, 0x00, hitzoneField) || !UserAddress(hitzoneField)
        || !reader.Read(hitzoneField, hitzone) || !UserAddress(hitzone)
        || !ReadHealthFloats(reader, hitzone, metadata.hp, metadata.maxHp)) return;
    metadata.healthValid = true;
    metadata.dead = metadata.hp <= 0.0f;
}

template<class Reader>
bool ReadPlayerPointers(const Reader& reader, std::uint64_t list, std::uint32_t count,
                        std::vector<std::uint64_t>& out) {
    std::vector<std::uint64_t> pointers;
    pointers.reserve(count);
    std::uint32_t index = 0;
    while (index + Config::EntityPointerBatchSize <= count) {
        std::uint64_t batchAddress{};
        std::array<std::uint64_t, Config::EntityPointerBatchSize> batch{};
        if (!AddAddress(list, std::uint64_t{index} * sizeof(std::uint64_t), batchAddress)
            || !UserAddress(batchAddress) || !reader.Read(batchAddress, batch)) return false;
        pointers.insert(pointers.end(), batch.begin(), batch.end());
        index += Config::EntityPointerBatchSize;
    }
    for (; index < count; ++index) {
        std::uint64_t slot{}, player{};
        if (!AddAddress(list, std::uint64_t{index} * sizeof(std::uint64_t), slot)
            || !UserAddress(slot) || !reader.Read(slot, player)) return false;
        pointers.push_back(player);
    }
    out = std::move(pointers);
    return true;
}

template<class Reader>
std::unordered_map<std::uint64_t, std::string> ReadPlayerNameMap(
        const Reader& reader, std::uint64_t game) {
    std::unordered_map<std::uint64_t, std::string> result;
    std::uint64_t managerField{}, manager{}, listField{}, list{}, countField{};
    std::uint32_t count{};
    constexpr std::uint32_t MaxPlayerCount = 512;
    if (!AddAddress(game, 0x2D8, managerField) || !UserAddress(managerField)
        || !reader.Read(managerField, manager) || !UserAddress(manager)
        || !AddAddress(manager, 0x18, listField) || !UserAddress(listField)
        || !reader.Read(listField, list) || !UserAddress(list)
        || !AddAddress(manager, 0x24, countField) || !UserAddress(countField)
        || !reader.Read(countField, count) || count > MaxPlayerCount) return result;
    std::vector<std::uint64_t> players;
    if (!ReadPlayerPointers(reader, list, count, players)) return result;
    result.reserve(count);
    for (const std::uint64_t player : players) {
        std::uint64_t keyField{}, key{}, nameField{};
        if (!UserAddress(player)
            || !AddAddress(player, 0x48, keyField) || !UserAddress(keyField)
            || !reader.Read(keyField, key) || !UserAddress(key)
            || !AddAddress(player, 0x18, nameField) || !UserAddress(nameField)) continue;
        std::array<unsigned char, 96> bytes{};
        const bool inlineRead = reader.Read(nameField, bytes);
        std::string name = inlineRead ? DecodeNarrowText(bytes) : std::string{};
        if (name.empty()) {
            std::uint64_t namePointer{};
            if (inlineRead) std::memcpy(&namePointer, bytes.data(), sizeof(namePointer));
            else reader.Read(nameField, namePointer);
            if (UserAddress(namePointer) && reader.Read(namePointer, bytes))
                name = DecodeNarrowText(bytes);
        }
        if (!name.empty()) result[key] = name;
    }
    return result;
}

template<class Reader>
bool FetchWorldLocalCharacter(const Reader& reader, std::uint64_t moduleBase,
                              std::uint64_t& localCharacter, Vector3& localPosition) {
    std::uint64_t rootSlot{}, game{}, worldField{}, world{}, localField{}, positionField{};
    WeakSnapshot local{};
    Vector3 position{};
    if (!Config::RootConfigured
        || !AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game) || !UserAddress(game)
        || !AddAddress(game, Config::World, worldField) || !UserAddress(worldField)
        || !reader.Read(worldField, world) || !UserAddress(world)
        || !AddAddress(world, Config::WorldLocalCharacter, localField)
        || !ReadWeakSnapshot(reader, localField, local)
        || !AddAddress(local.object, Config::EntityWorldPosition, positionField)
        || !UserAddress(positionField) || !reader.Read(positionField, position)
        || !Finite(position) || !NonZero(position)) return false;

    std::uint64_t gameAgain{}, worldAgain{};
    if (!reader.Read(rootSlot, gameAgain) || gameAgain != game
        || !reader.Read(worldField, worldAgain) || worldAgain != world
        || !WeakUnchanged(reader, local)) return false;
    localCharacter = local.object;
    localPosition = position;
    return true;
}

bool WildEntityCountChange(std::uint32_t previous, std::uint32_t current) {
    const std::uint64_t difference = previous > current
        ? std::uint64_t{previous} - current : std::uint64_t{current} - previous;
    return difference > Config::EntityCountJumpAbsolute
        && difference * 2 > (std::max)(std::uint64_t{previous}, std::uint64_t{1});
}

struct ScanDiagnosticSnapshot {
    std::uint32_t rawWorldCount = 0;
    std::size_t validPointers = 0;
    std::size_t deduplicatedPointers = 0;
    std::size_t classMatches = 0;
    std::size_t validPositions = 0;
    std::size_t distanceMatches = 0;
    bool localAnchorValid = false;
    Vector3 localAnchor{};
    const char* rejection = "none";
    bool published = false;
    std::size_t publishedCharacters = 0;
};

void RecordScanStatistics(bool accepted, const ScanDiagnosticSnapshot& snapshot) {
    g_readStatistics.lastWorldCount = snapshot.rawWorldCount;
    g_readStatistics.lastScanPartial =
        snapshot.rawWorldCount > Config::FullEntityScanLimit;
    if (accepted) {
        ++g_readStatistics.acceptedScans;
        g_readStatistics.consecutiveRejectedScans = 0;
        g_readStatistics.lastAcceptedScanTick = GetTickCount64();
        if (g_readStatistics.lastScanPartial) g_readStatistics.consecutiveFullScans = 0;
        else ++g_readStatistics.consecutiveFullScans;
    } else {
        ++g_readStatistics.rejectedScans;
        ++g_readStatistics.consecutiveRejectedScans;
        g_readStatistics.consecutiveFullScans = 0;
    }
}

bool RejectScan(ScanDiagnosticSnapshot* diagnostic, const char* reason,
                const char* logMessage) {
    if (diagnostic) diagnostic->rejection = reason;
    DiagnosticLog("[entity-scan] rejected: %s\n", logMessage);
    return false;
}

template<class Reader>
bool RefreshCharacterEntities(const Reader& reader, std::uint64_t moduleBase,
                               CharacterScanState& state,
                               ScanDiagnosticSnapshot* diagnostic = nullptr) {
    if (diagnostic) *diagnostic = {};
    std::uint64_t rootSlot{}, game{}, worldField{}, world{}, dataField{}, countField{}, data{};
    std::uint64_t localField{}, localPositionField{};
    WeakSnapshot local{};
    Vector3 localPosition{};
    std::uint32_t count{};
    if (!Config::RootConfigured)
        return RejectScan(diagnostic, "root_not_configured", "root is not configured");
    if (!AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot))
        return RejectScan(diagnostic, "root_slot_invalid", "root slot address invalid");
    if (!reader.Read(rootSlot, game) || !UserAddress(game))
        return RejectScan(diagnostic, "game_read_failed", "game pointer read failed or invalid");
    if (!AddAddress(game, Config::World, worldField) || !UserAddress(worldField)
        || !reader.Read(worldField, world) || !UserAddress(world))
        return RejectScan(diagnostic, "world_read_failed", "world pointer read failed or invalid");
    if (!AddAddress(world, Config::EntityArrayData, dataField) || !UserAddress(dataField)
        || !AddAddress(world, Config::EntityArrayCount, countField) || !UserAddress(countField)
        || !reader.Read(dataField, data) || !UserAddress(data)
        || !reader.Read(countField, count))
        return RejectScan(diagnostic, "world_list_header_failed",
                          "world list data/count read failed or invalid");
    if (diagnostic) diagnostic->rawWorldCount = count;
    if (!AddAddress(world, Config::WorldLocalCharacter, localField)
        || !ReadWeakSnapshot(reader, localField, local))
        return RejectScan(diagnostic, "local_weak_failed", "local weak reference failed");
    if (!AddAddress(local.object, Config::EntityWorldPosition, localPositionField)
        || !UserAddress(localPositionField) || !reader.Read(localPositionField, localPosition))
        return RejectScan(diagnostic, "local_position_read_failed", "local position read failed");
    if (!Finite(localPosition) || !NonZero(localPosition))
        return RejectScan(diagnostic, "local_position_invalid",
                          "local position is nonfinite or zero");
    if (diagnostic) {
        diagnostic->localAnchorValid = true;
        diagnostic->localAnchor = localPosition;
    }
    if (count > Config::FullEntityScanLimit) {
        if (diagnostic) diagnostic->rejection = "count_limit";
        DiagnosticLog("[entity-scan] rejected: count=%u exceeds safety_limit=%u\n",
            count, Config::FullEntityScanLimit);
        return false;
    }
    if (state.hasAcceptedCount && WildEntityCountChange(state.lastCount, count)) {
        if (diagnostic) diagnostic->rejection = "wild_count_change";
        DiagnosticLog("[entity-scan] rejected: wild count change previous=%u current=%u\n",
            state.lastCount, count);
        return false;
    }

    std::vector<std::uint64_t> pointers;
    pointers.reserve(count);
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(count);
    std::uint32_t index = 0;
    while (index + Config::EntityPointerBatchSize <= count) {
        std::uint64_t batchAddress{};
        std::array<std::uint64_t, Config::EntityPointerBatchSize> batch{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), batchAddress)
            || !UserAddress(batchAddress) || !reader.Read(batchAddress, batch)) {
            if (diagnostic) diagnostic->rejection = "pointer_batch_read_failed";
            DiagnosticLog("[entity-scan] rejected: pointer batch read failed at index=%u\n", index);
            return false;
        }
        for (const auto entity : batch) {
            if (UserAddress(entity)) {
                if (diagnostic) ++diagnostic->validPointers;
                if (seen.insert(entity).second) pointers.push_back(entity);
            }
        }
        index += Config::EntityPointerBatchSize;
    }
    for (; index < count; ++index) {
        std::uint64_t entryAddress{}, entity{};
        if (!AddAddress(data, std::uint64_t{index} * sizeof(std::uint64_t), entryAddress)
            || !UserAddress(entryAddress) || !reader.Read(entryAddress, entity)) {
            if (diagnostic) diagnostic->rejection = "pointer_tail_read_failed";
            DiagnosticLog("[entity-scan] rejected: pointer read failed at index=%u\n", index);
            return false;
        }
        if (UserAddress(entity)) {
            if (diagnostic) ++diagnostic->validPointers;
            if (seen.insert(entity).second) pointers.push_back(entity);
        }
    }
    if (diagnostic) diagnostic->deduplicatedPointers = pointers.size();

    const auto playerNames = ReadPlayerNameMap(reader, game);
    std::unordered_map<std::uint64_t, std::string> newClasses;
    std::unordered_map<std::uint64_t, CharacterVisualMetadata> newMetadata;
    std::vector<std::uint64_t> characters;
    for (const auto entity : pointers) {
        const std::string* className = nullptr;
        const auto cached = state.classCache.find(entity);
        if (cached != state.classCache.end()) {
            className = &cached->second;
        } else {
            bool unreadableText = false;
            std::string rawHex, resolved;
            if (!ReadClassName(reader, entity, resolved, unreadableText, rawHex)
                || unreadableText) continue;
            const auto inserted = newClasses.emplace(entity, std::move(resolved));
            className = &inserted.first->second;
        }
        if (*className != Config::CharacterClassName) continue;
        if (diagnostic) ++diagnostic->classMatches;

        std::uint64_t positionField{};
        Vector3 position{};
        if (!AddAddress(entity, Config::EntityWorldPosition, positionField)
            || !UserAddress(positionField) || !reader.Read(positionField, position)
            || !Finite(position) || !NonZero(position)) continue;
        if (diagnostic) ++diagnostic->validPositions;
        const double distance = Distance(position, localPosition);
        if (!std::isfinite(distance) || distance > Config::CharacterDistanceLimit) continue;
        if (diagnostic) ++diagnostic->distanceMatches;

        CharacterVisualMetadata metadata{};
        metadata.faction = ReadFactionForOverlay(reader, entity);
        ReadHealthForOverlay(reader, entity, metadata);
        std::uint64_t entityKeyField{}, entityKey{};
        if (AddAddress(entity, 0x20, entityKeyField) && UserAddress(entityKeyField)
            && reader.Read(entityKeyField, entityKey) && UserAddress(entityKey)) {
            const auto player = playerNames.find(entityKey);
            if (player != playerNames.end()) metadata.playerName = player->second;
        }
        newMetadata.emplace(entity, std::move(metadata));
        characters.push_back(entity);
    }

    std::uint64_t gameAgain{}, worldAgain{}, dataAgain{};
    if (!reader.Read(rootSlot, gameAgain) || gameAgain != game)
        return RejectScan(diagnostic, "root_changed", "root changed during scan");
    if (!reader.Read(worldField, worldAgain) || worldAgain != world)
        return RejectScan(diagnostic, "world_changed", "world changed during scan");
    if (!reader.Read(dataField, dataAgain) || dataAgain != data)
        return RejectScan(diagnostic, "data_changed", "world list data changed during scan");
    if (!WeakUnchanged(reader, local))
        return RejectScan(diagnostic, "local_changed", "local weak reference changed during scan");

    for (auto& entry : newClasses) state.classCache.emplace(entry.first, std::move(entry.second));
    for (auto it = state.classCache.begin(); it != state.classCache.end();) {
        if (seen.find(it->first) == seen.end()) it = state.classCache.erase(it);
        else ++it;
    }
    state.entities = std::move(characters);
    state.metadataCache = std::move(newMetadata);
    state.lastCount = count;
    state.hasAcceptedCount = true;
    if (diagnostic) {
        diagnostic->published = true;
        diagnostic->publishedCharacters = state.entities.size();
    }
    std::size_t us = 0, ussr = 0, fia = 0, civ = 0, unknown = 0, players = 0;
    for (const auto& entry : state.metadataCache) {
        if (entry.second.faction == "US") ++us;
        else if (entry.second.faction == "USSR") ++ussr;
        else if (entry.second.faction == "FIA") ++fia;
        else if (entry.second.faction == "CIV") ++civ;
        else ++unknown;
        if (!entry.second.playerName.empty()) ++players;
    }
    DiagnosticLog("[entity-scan] characters=%zu factions{US=%zu USSR=%zu FIA=%zu CIV=%zu Unknown=%zu} players=%zu\n",
        state.entities.size(), us, ussr, fia, civ, unknown, players);
    return true;
}

void RunScanDiagnostic(const PlaceholderReader& reader, std::uint64_t moduleBase) {
    CharacterScanState state{};
    std::uint64_t iteration = 0;
    constexpr std::uint64_t MaxIterations = 10;
    DiagnosticLog("[scan-diagnostic] begin range=%.1f samples=%llu; press End to stop\n",
                  static_cast<double>(Config::CharacterDistanceLimit),
                  static_cast<unsigned long long>(MaxIterations));
    while (iteration < MaxIterations && !(GetAsyncKeyState(VK_END) & 0x8000)) {
        ScanDiagnosticSnapshot snapshot{};
        const bool accepted = RefreshCharacterEntities(reader, moduleBase, state, &snapshot);
        if (snapshot.localAnchorValid) {
            DiagnosticLog("[scan-diagnostic] iteration=%llu raw=%u valid_ptr=%zu "
                "dedupe=%zu class=%zu position=%zu distance=%zu "
                "local_anchor=valid(%.3f,%.3f,%.3f) result=%s rejection=%s "
                "published=%zu retained=%zu range=%.1f\n",
                static_cast<unsigned long long>(iteration), snapshot.rawWorldCount,
                snapshot.validPointers, snapshot.deduplicatedPointers,
                snapshot.classMatches, snapshot.validPositions, snapshot.distanceMatches,
                static_cast<double>(snapshot.localAnchor.x),
                static_cast<double>(snapshot.localAnchor.y),
                static_cast<double>(snapshot.localAnchor.z),
                accepted ? "accepted" : "rejected", snapshot.rejection,
                snapshot.publishedCharacters, state.entities.size(),
                static_cast<double>(Config::CharacterDistanceLimit));
        } else {
            DiagnosticLog("[scan-diagnostic] iteration=%llu raw=%u valid_ptr=%zu "
                "dedupe=%zu class=%zu position=%zu distance=%zu "
                "local_anchor=invalid result=%s rejection=%s published=%zu "
                "retained=%zu range=%.1f\n",
                static_cast<unsigned long long>(iteration), snapshot.rawWorldCount,
                snapshot.validPointers, snapshot.deduplicatedPointers,
                snapshot.classMatches, snapshot.validPositions, snapshot.distanceMatches,
                accepted ? "accepted" : "rejected", snapshot.rejection,
                snapshot.publishedCharacters, state.entities.size(),
                static_cast<double>(Config::CharacterDistanceLimit));
        }
        ++iteration;
        Sleep(Config::EntityScanIntervalMs);
    }
    DiagnosticLog("[scan-diagnostic] end\n");
}

// Independently established entity layout: embedded world translation, not local
// translation, controller translation, or a skeleton bone. No height correction.
// Reacquire on every frame. Rechecks detect some transitions, not lifetime/ABA races.
template<class Reader>
bool FetchLocalEntityPosition(const Reader& reader, std::uint64_t moduleBase, Vector3& out) {
    if (!Config::RootConfigured) return false;
    std::uint64_t slot{}, game{}, controllerField{}, entityField{}, positionField{};
    WeakSnapshot controller{}, entity{};
    Vector3 position{};
    if (!AddAddress(moduleBase, Config::GameBase, slot)
        || !reader.Read(slot, game) || !UserAddress(game)
        || !AddAddress(game, Config::LocalPlayerController, controllerField)
        || !ReadWeakSnapshot(reader, controllerField, controller)
        || !AddAddress(controller.object, Config::ControlledEntity, entityField)
        || !ReadWeakSnapshot(reader, entityField, entity)
        || !AddAddress(entity.object, Config::EntityWorldPosition, positionField)
        || !reader.Read(positionField, position) || !Finite(position)) return false;
    std::uint64_t gameAgain{};
    if (!reader.Read(slot, gameAgain) || gameAgain != game
        || !WeakUnchanged(reader, controller) || !WeakUnchanged(reader, entity)) return false;
    out = position; // Publish only a complete, accepted sample; no (0,0,0) fallback.
    return true;
}

struct HeadBoneProbe {
    std::uint64_t entity{}, matrixBase{}, boneAddress{};
    Vector3 local{}, world{};
};

// Diagnostic-only candidate probe. Index 83 comes from the supplied content map;
// the executable proves the packed matrix path and transform math, not that this
// index is the head for every character asset. Nothing is written to the target.
template<class Reader>
bool FetchLocalHeadBoneCandidate(const Reader& reader, std::uint64_t moduleBase,
                                 HeadBoneProbe& out) {
    if (!Config::RootConfigured) return false;
    std::uint64_t slot{}, game{}, controllerField{}, entityField{};
    std::uint64_t meshField{}, mesh{}, matrixOwnerField{}, matrixOwner{};
    std::uint64_t matrixBaseField{}, matrixBase{}, boneOffset{}, boneAddress{};
    WeakSnapshot controller{}, entity{};
    Matrix3x4 bone{}, entityTransform{};
    if (!AddAddress(moduleBase, Config::GameBase, slot)
        || !reader.Read(slot, game) || !UserAddress(game)
        || !AddAddress(game, Config::LocalPlayerController, controllerField)
        || !ReadWeakSnapshot(reader, controllerField, controller)
        || !AddAddress(controller.object, Config::ControlledEntity, entityField)
        || !ReadWeakSnapshot(reader, entityField, entity)
        || !AddAddress(entity.object, Config::MeshObjectComponent, meshField)
        || !reader.Read(meshField, mesh) || !UserAddress(mesh)
        || !AddAddress(mesh, 0x30, matrixOwnerField)
        || !reader.Read(matrixOwnerField, matrixOwner)) return false;

    if (matrixOwner) {
        if (!UserAddress(matrixOwner)
            || !AddAddress(matrixOwner, 0x10, matrixBaseField)
            || !reader.Read(matrixBaseField, matrixBase)) return false;
    } else {
        std::uint64_t meshLinkField{}, meshLink{}, meshDataField{}, meshData{};
        std::uint64_t skeletonField{}, skeleton{};
        if (!AddAddress(mesh, 0x10, meshLinkField)
            || !reader.Read(meshLinkField, meshLink) || !UserAddress(meshLink)
            || !AddAddress(meshLink, 0x30, meshDataField)
            || !reader.Read(meshDataField, meshData) || !UserAddress(meshData)
            || !AddAddress(meshData, 0xD0, skeletonField)
            || !reader.Read(skeletonField, skeleton) || !UserAddress(skeleton)
            || !AddAddress(skeleton, 0x10, matrixBaseField)
            || !reader.Read(matrixBaseField, matrixBase)) return false;
    }
    if (!UserAddress(matrixBase)
        || Config::HeadBoneIndexCandidate >
            (std::numeric_limits<std::uint64_t>::max)() / Config::BoneMatrixStride)
        return false;
    boneOffset = std::uint64_t{Config::HeadBoneIndexCandidate} * Config::BoneMatrixStride;
    std::uint64_t transformAddress{};
    if (!AddAddress(matrixBase, boneOffset, boneAddress)
        || !reader.Read(boneAddress, bone) || !Finite(bone)
        || !AddAddress(entity.object, Config::EntityTransform, transformAddress)
        || !reader.Read(transformAddress, entityTransform) || !Finite(entityTransform)) return false;

    HeadBoneProbe sample{};
    sample.entity = entity.object;
    sample.matrixBase = matrixBase;
    sample.boneAddress = boneAddress;
    sample.local = bone.translation;
    sample.world = TransformPoint(entityTransform, sample.local);
    if (!Finite(sample.world)) return false;

    std::uint64_t gameAgain{}, meshAgain{}, matrixBaseAgain{};
    if (!reader.Read(slot, gameAgain) || gameAgain != game
        || !WeakUnchanged(reader, controller) || !WeakUnchanged(reader, entity)
        || !reader.Read(meshField, meshAgain) || meshAgain != mesh
        || !reader.Read(matrixBaseField, matrixBaseAgain) || matrixBaseAgain != matrixBase)
        return false;
    out = sample;
    return true;
}

// Observed on three 231-bone Chimera characters. The arm chains begin at
// 45/195; the leg chains at 18/25. Indices denote anatomical side in model
// space, which can differ from screen left/right as the entity rotates.
enum SkeletonJoint : std::size_t {
    Pelvis, SpineLow, SpineMid, SpineHigh, Neck, Head,
    ShoulderA, ElbowA, HandA, ShoulderB, ElbowB, HandB,
    HipA, KneeA, FootA, HipB, KneeB, FootB, JointCount
};
constexpr std::array<std::uint32_t, JointCount> SkeletonIndices{
    10, 31, 36, 41, 81, 83, 45, 48, 51, 195, 198, 201,
    18, 19, 23, 25, 26, 30
};
struct SkeletonWorldSample { std::array<Vector3, JointCount> joints{}; };
struct SkeletonMeshLinks {
    std::uint64_t data{};
    std::byte reserved[0x18]{};
    std::uint64_t owner{};
};
static_assert(sizeof(SkeletonMeshLinks) == 0x28);

template<class Reader>
bool FetchCharacterSkeleton(const Reader& reader, std::uint64_t entity,
                            SkeletonWorldSample& out) {
    std::uint64_t meshField{}, mesh{}, linksField{}, owner{}, baseField{}, base{};
    std::uint64_t data{}, objectField{}, object{}, skeletonField{}, skeleton{};
    std::uint64_t countField{}, transformField{};
    std::uint32_t count{};
    Matrix3x4 transform{};
    SkeletonMeshLinks links{};
    if (!UserAddress(entity)
        || !AddAddress(entity, Config::MeshObjectComponent, meshField)
        || !UserAddress(meshField) || !reader.Read(meshField, mesh) || !UserAddress(mesh)
        || !AddAddress(mesh, Config::MeshComponentData, linksField)
        || !UserAddress(linksField) || !reader.Read(linksField, links)
        || !UserAddress(links.data) || !UserAddress(links.owner)) return false;
    owner = links.owner;
    data = links.data;
    if (!AddAddress(owner, Config::MatrixOwnerArray, baseField) || !UserAddress(baseField)
        || !reader.Read(baseField, base) || !UserAddress(base)
        || !AddAddress(data, Config::MeshDataObject, objectField) || !UserAddress(objectField)
        || !reader.Read(objectField, object) || !UserAddress(object)
        || !AddAddress(object, Config::MeshObjectSkeleton, skeletonField) || !UserAddress(skeletonField)
        || !reader.Read(skeletonField, skeleton) || !UserAddress(skeleton)
        || !AddAddress(skeleton, Config::SkeletonBoneCount, countField) || !UserAddress(countField)
        || !reader.Read(countField, count) || count < Config::SkeletonMinimumCount
        || count > 512
        || !AddAddress(entity, Config::EntityTransform, transformField)
        || !UserAddress(transformField) || !reader.Read(transformField, transform)
        || !Finite(transform)) return false;

    // Requested joints occupy two disjoint contiguous spans: 10..83 and
    // 195..201. Two reads transfer 3,888 rather than 9,216 bytes.
    std::array<Matrix3x4, 74> bodyAndArmA{};
    std::array<Matrix3x4, 7> armB{};
    std::uint64_t firstAddress{}, secondAddress{};
    if (!AddAddress(base, 10ull * Config::BoneMatrixStride, firstAddress)
        || !AddAddress(base, 195ull * Config::BoneMatrixStride, secondAddress)
        || !UserAddress(firstAddress) || !UserAddress(secondAddress)
        || !reader.Read(firstAddress, bodyAndArmA)
        || !reader.Read(secondAddress, armB)) return false;
    SkeletonWorldSample sample{};
    for (std::size_t i = 0; i < JointCount; ++i) {
        const auto index = SkeletonIndices[i];
        const Matrix3x4& bone = index <= 83 ? bodyAndArmA[index - 10]
            : armB[index - 195];
        if (!Finite(bone)) return false;
        sample.joints[i] = TransformPoint(transform, bone.translation);
        if (!Finite(sample.joints[i])) return false;
    }
    std::uint64_t meshAgain{}, baseAgain{}, objectAgain{}, skeletonAgain{};
    SkeletonMeshLinks linksAgain{};
    std::uint32_t countAgain{};
    if (!reader.Read(meshField, meshAgain) || meshAgain != mesh
        || !reader.Read(linksField, linksAgain)
        || linksAgain.owner != owner || linksAgain.data != data
        || !reader.Read(baseField, baseAgain) || baseAgain != base
        || !reader.Read(objectField, objectAgain) || objectAgain != object
        || !reader.Read(skeletonField, skeletonAgain) || skeletonAgain != skeleton
        || !reader.Read(countField, countAgain) || countAgain != count) return false;
    out = sample;
    return true;
}
#pragma pack(push, 1)
struct CameraReadBlock {
    Vector3 right;
    Vector3 up;
    Vector3 forward;
    Vector3 position;
    std::byte reserved[0x194 - 0xB0];
    float zoomScale;
};
#pragma pack(pop)
static_assert(offsetof(CameraReadBlock, right) == 0x00);
static_assert(offsetof(CameraReadBlock, up) == 0x0C);
static_assert(offsetof(CameraReadBlock, forward) == 0x18);
static_assert(offsetof(CameraReadBlock, position) == 0x24);
static_assert(offsetof(CameraReadBlock, zoomScale) == 0x114);
static_assert(sizeof(CameraReadBlock) == 0x118);

template<class Reader>
bool ReadCameraBlock(const Reader& reader, std::uint64_t currentCamera, Camera& out) {
    std::uint64_t blockAddress{};
    CameraReadBlock block{};
    if (!UserAddress(currentCamera)
        || !AddAddress(currentCamera, 0x80, blockAddress) || !UserAddress(blockAddress)
        || !reader.Read(blockAddress, block)
        || !Finite(block.position) || !NonZero(block.position)
        || !Finite(block.right) || !Finite(block.up)
        || !Finite(block.forward) || !std::isfinite(block.zoomScale)
        || block.zoomScale <= 1e-6f) return false;
    Camera camera{block.position, block.right, block.up, block.forward, block.zoomScale};
    out = camera;
    return true;
}

template<class Reader>
bool FetchCamera(const Reader& reader, std::uint64_t moduleBase, Camera& out) {
    if (!Config::RootConfigured) return false;
    std::uint64_t rootSlot{}, managerWeak{}, manager{}, cameraWeak{}, currentCamera{};
    if (!AddAddress(moduleBase, Config::GameBase, rootSlot)) return false;
    // Driver semantics: current = *(uint64_t*)current + offset.
    // This reads the root pointer with zero offsets, then locates its weak field.
    std::uint64_t root{};
    if (!reader.Read(rootSlot, root) || !AddAddress(root, 0x328, managerWeak)
        || !ResolveWeak(reader, managerWeak, manager)
        || !AddAddress(manager, 0x110, cameraWeak)
        || !ResolveWeak(reader, cameraWeak, currentCamera)) return false;
    // +0x110 resolves CurrentCamera; its dynamic type is not guaranteed to be PlayerCamera.
    // These transform fields belong to the CameraBase layout in the analyzed build.
    return ReadCameraBlock(reader, currentCamera, out);
}

struct GmCameraSample { Vector3 position{}, forward{}; };
struct GmScanState {
    std::uint64_t entity = 0;
    std::uint64_t entitySlot = 0;
    std::unordered_set<std::uint64_t> checked;
    std::uint64_t listData = 0;
    std::uint32_t listCount = 0, nextIndex = 0;
};

template<class Reader>
bool DiscoverGmEntity(const Reader& reader, std::uint64_t moduleBase,
                      GmScanState& state) {
    if (UserAddress(state.entity)) {
        std::string name, raw;
        bool unreadable{};
        std::uint64_t listed{};
        if (UserAddress(state.entitySlot) && reader.Read(state.entitySlot, listed)
            && listed == state.entity
            && ReadClassName(reader, state.entity, name, unreadable, raw) && !unreadable
            && (name == Config::GmClassName
                || name == "SCR_EditorManagerEntity")) return true;
        state.entity = 0;
        state.entitySlot = 0;
        state.checked.clear();
        state.nextIndex = 0;
    }
    std::uint64_t rootSlot{}, game{}, world{}, data{};
    std::uint32_t count{};
    if (!AddAddress(moduleBase, Config::GameBase, rootSlot) || !UserAddress(rootSlot)
        || !reader.Read(rootSlot, game) || !UserAddress(game)
        || !reader.Read(game + Config::World, world) || !UserAddress(world)
        || !reader.Read(world + Config::EntityArrayData, data) || !UserAddress(data)
        || !reader.Read(world + Config::EntityArrayCount, count)
        || count > Config::FullEntityScanLimit) return false;
    if (data != state.listData || count != state.listCount) {
        state.checked.clear();
        state.listData = data;
        state.listCount = count;
        state.nextIndex = 0;
    }
    std::uint64_t found{};
    constexpr std::uint32_t MaxEntriesPerScan = 256;
    std::uint32_t examined = 0;
    while (state.nextIndex < count && examined < MaxEntriesPerScan && !found) {
        std::array<std::uint64_t, Config::EntityPointerBatchSize> batch{};
        const std::uint32_t batchStart = state.nextIndex;
        const std::uint32_t remaining = count - state.nextIndex;
        if (remaining >= Config::EntityPointerBatchSize) {
            std::uint64_t address{};
            if (!AddAddress(data, std::uint64_t{state.nextIndex} * 8, address)
                || !UserAddress(address) || !reader.Read(address, batch)) return false;
            state.nextIndex += Config::EntityPointerBatchSize;
            examined += Config::EntityPointerBatchSize;
        } else {
            for (std::uint32_t i = 0; i < remaining; ++i) {
                std::uint64_t address{};
                if (!AddAddress(data, std::uint64_t{state.nextIndex + i} * 8, address)
                    || !UserAddress(address) || !reader.Read(address, batch[i])) return false;
            }
            state.nextIndex = count;
            examined += remaining;
        }
        for (std::size_t i = 0; i < batch.size(); ++i) {
            const auto entity = batch[i];
            if (!UserAddress(entity) || !state.checked.insert(entity).second) continue;
            std::string name, raw;
            bool unreadable{};
            if (ReadClassName(reader, entity, name, unreadable, raw) && !unreadable) {
                if (name == Config::GmClassName
                    || name == "SCR_EditorManagerEntity") {
                    found = entity;
                    if (!AddAddress(data, std::uint64_t{batchStart + i} * 8, state.entitySlot)
                        || !UserAddress(state.entitySlot)) return false;
                    break;
                }
            }
        }
    }
    std::uint64_t gameAgain{}, worldAgain{}, dataAgain{};
    std::uint32_t countAgain{};
    if (!reader.Read(rootSlot, gameAgain) || gameAgain != game
        || !reader.Read(game + Config::World, worldAgain) || worldAgain != world
        || !reader.Read(world + Config::EntityArrayData, dataAgain) || dataAgain != data
        || !reader.Read(world + Config::EntityArrayCount, countAgain) || countAgain != count)
        return false;
    if (!found && state.nextIndex == count) {
        state.nextIndex = 0;
        state.checked.clear();
    }
    state.entity = found;
    return UserAddress(found);
}

struct GmComponentBlock {
    float qx, qy, qz, qw;
    std::byte reserved[8];
    Vector3 position;
};
static_assert(sizeof(GmComponentBlock) == 36);

struct RttiLocator {
    std::uint32_t signature, offset, cdOffset;
    std::int32_t typeRva, hierarchyRva, selfRva;
};
static_assert(sizeof(RttiLocator) == 24);

template<class Reader>
bool NormalizeNwkComponent(const Reader& reader, std::uint64_t subobject,
                           std::uint64_t& complete) {
    std::uint64_t vtable{}, locatorAddress{};
    RttiLocator locator{};
    if (!UserAddress(subobject) || !reader.Read(subobject, vtable)
        || vtable < 8 || !UserAddress(vtable - 8)
        || !reader.Read(vtable - 8, locatorAddress) || !UserAddress(locatorAddress)
        || !reader.Read(locatorAddress, locator) || locator.signature != 1
        || locator.offset > 0x1000 || locator.selfRva < 0
        || locator.typeRva < 0 || locatorAddress < std::uint32_t(locator.selfRva))
        return false;
    const std::uint64_t imageBase = locatorAddress - std::uint32_t(locator.selfRva);
    std::uint64_t typeAddress{}, nameAddress{};
    if (!AddAddress(imageBase, std::uint32_t(locator.typeRva), typeAddress)
        || !AddAddress(typeAddress, 16, nameAddress) || !UserAddress(nameAddress))
        return false;
    std::array<unsigned char, 96> name{};
    if (!reader.Read(nameAddress, name)
        || DecodeNarrowText(name).find("NwkMovementComponent") == std::string::npos
        || subobject < locator.offset || !UserAddress(subobject - locator.offset))
        return false;
    complete = subobject - locator.offset;
    return true;
}

template<class Reader>
bool FetchGmCamera(const Reader& reader, std::uint64_t entity,
                   GmCameraSample& out) {
    std::uint64_t components{}, component{}, originalComponent{};
    std::uint64_t componentsAgain{}, componentAgain{};
    std::uint32_t count{}, countAgain{};
    std::uint64_t active{}, activeAgain{};
    Matrix3x4 entityTransform{}, transformAgain{};
    if (!UserAddress(entity)
        || !reader.Read(entity + Config::GmActiveFlag, active) || active != 1
        || !reader.Read(entity + Config::GmComponents, components) || !UserAddress(components)
        || !reader.Read(entity + Config::GmComponentCount, count) || count == 0 || count > 256
        || !reader.Read(entity + Config::EntityTransform, entityTransform)
        || !Finite(entityTransform)) return false;
    std::uint32_t componentIndex = Config::GmPreferredMovementIndex;
    if (count > componentIndex) reader.Read(components + std::uint64_t{componentIndex} * 8, component);
    originalComponent = component;
    if (!UserAddress(component)) {
        component = 0;
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint64_t candidate{}, normalized{};
            if (!reader.Read(components + std::uint64_t{i} * 8, candidate)
                || !NormalizeNwkComponent(reader, candidate, normalized)) continue;
            component = normalized;
            originalComponent = candidate;
            componentIndex = i;
            break;
        }
    }
    GmComponentBlock block{};
    const bool haveBlock = UserAddress(component)
        && reader.Read(component + Config::GmMovementQuaternion, block);
    const double norm = double(block.qx)*block.qx + double(block.qy)*block.qy
        + double(block.qz)*block.qz + double(block.qw)*block.qw;
    const double x = block.qx, y = block.qy, z = block.qz, w = block.qw;
    Vector3 forward{float(2.0*(x*z + w*y)), float(2.0*(y*z - w*x)),
                    float(1.0 - 2.0*(x*x + y*y))};
    const bool snapshotValid = haveBlock && std::isfinite(norm) && norm >= 0.9
        && norm <= 1.1 && Finite(block.position) && NonZero(block.position)
        && Finite(forward) && Distance(block.position, entityTransform.translation) <= 1.0
        && Dot(forward, entityTransform.forward) >= 0.9;
    if (!reader.Read(entity + Config::GmActiveFlag, activeAgain)
        || activeAgain != active
        || !reader.Read(entity + Config::GmComponents, componentsAgain)
        || componentsAgain != components
        || !reader.Read(entity + Config::GmComponentCount, countAgain)
        || countAgain != count
        || !reader.Read(entity + Config::EntityTransform, transformAgain)
        || std::memcmp(&transformAgain, &entityTransform, sizeof(transformAgain)) != 0
        || (UserAddress(component) && (!reader.Read(
            components + std::uint64_t{componentIndex} * 8, componentAgain)
            || componentAgain != originalComponent))) return false;
    if (snapshotValid) out = {block.position, forward};
    else if (Finite(entityTransform.translation) && NonZero(entityTransform.translation)
        && Finite(entityTransform.forward))
        out = {entityTransform.translation, entityTransform.forward};
    else return false;
    return true;
}

bool WorldToScreen(Vector3 world, const Camera& c, int width, int height,
                   float& outX, float& outY) {
    if (width <= 0 || height <= 0 || !Finite(world) || !Finite(c.position)
        || !Finite(c.right) || !Finite(c.up) || !Finite(c.forward)
        || !std::isfinite(c.zoomScale) || c.zoomScale <= 1e-6f) return false;
    // Assumes orthonormal camera axes and positive Forward toward the scene.
    const Vector3 relative = world - c.position;
    const double x = Dot(relative, c.right), y = Dot(relative, c.up);
    const double z = Dot(relative, c.forward);
    if (!std::isfinite(z) || z <= 1e-4) return false; // Behind/too near camera.
    // GetVerticalFOV computes atan(camera+0x194) + atan(camera+0x198), while
    // SetVerticalFOV writes tan(verticalFov/2) to both. For the symmetric case
    // +0x194 is therefore the vertical half-frustum tangent. Derive horizontal
    // tangent from the square-pixel viewport aspect ratio.
    const double tanV = c.zoomScale;
    const double tanH = tanV * double(width) / height;
    const double ndcX = x / (z * tanH), ndcY = y / (z * tanV);
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)
        || std::abs(ndcX) > 1 || std::abs(ndcY) > 1) return false;
    // Convert centered normalized coordinates to top-left pixel coordinates.
    outX = float((ndcX + 1) * 0.5 * width);
    outY = float((1 - ndcY) * 0.5 * height);
    return true;
}

struct ScreenCharacter {
    bool visible = true;
    float feetX = 0, feetY = 0;
    float headX = 0, headY = 0;
    Vector3 worldPosition{};
    double distance = 0;
    bool local = false;
    std::string faction = "Unknown";
    std::string playerName;
    float hp = 0, maxHp = 0;
    bool healthValid = false;
    bool skeletonSampleValid = false;
    bool skeletonValid = false;
    SkeletonWorldSample skeletonWorld{};
    std::array<ImVec2, JointCount> skeleton{};
};

bool ProjectSkeleton(const SkeletonWorldSample& world, const Camera& camera,
                     int width, int height, ScreenCharacter& marker) {
    std::array<ImVec2, JointCount> projected{};
    for (std::size_t i = 0; i < JointCount; ++i) {
        if (!WorldToScreen(world.joints[i], camera, width, height,
                           projected[i].x, projected[i].y)) return false;
    }
    marker.skeleton = projected;
    marker.skeletonValid = true;
    return true;
}

bool ProjectCharacterGeometry(Vector3 position, const Camera& camera, int width, int height,
                              ScreenCharacter& out) {
    Vector3 headPosition = position;
    headPosition.y += Config::CharacterHeight;
    ScreenCharacter projected{};
    if (!WorldToScreen(position, camera, width, height, projected.feetX, projected.feetY)
        || !WorldToScreen(headPosition, camera, width, height,
                         projected.headX, projected.headY)
        || projected.feetY <= projected.headY) return false;
    out = projected;
    return true;
}

void RunFrameDiagnostic(const PlaceholderReader& reader, std::uint64_t moduleBase) {
    CharacterScanState state{};
    constexpr std::uint64_t MaxIterations = 10;
    DiagnosticLog("[frame-diagnostic] begin range=%.1f samples=%llu; press End to stop\n",
        static_cast<double>(Config::CharacterDistanceLimit),
        static_cast<unsigned long long>(MaxIterations));
    for (std::uint64_t iteration = 0;
         iteration < MaxIterations && !(GetAsyncKeyState(VK_END) & 0x8000);
         ++iteration) {
        ScanDiagnosticSnapshot scan{};
        const bool scanAccepted = RefreshCharacterEntities(reader, moduleBase, state, &scan);
        HWND gameWindow = FindWindowW(nullptr, Config::WindowTitle);
        RECT viewport{};
        const bool viewportValid = gameWindow && GetClientRect(gameWindow, &viewport)
            && viewport.right > 0 && viewport.bottom > 0;
        Camera camera{};
        const bool cameraValid = viewportValid && FetchCamera(reader, moduleBase, camera);
        std::uint64_t localCharacter{};
        Vector3 localPosition{};
        const bool localValid = cameraValid && FetchWorldLocalCharacter(
            reader, moduleBase, localCharacter, localPosition);

        std::size_t localSkipped = 0, deadSkipped = 0, positionSkipped = 0;
        std::size_t distanceSkipped = 0, projectionSkipped = 0, renderable = 0;
        if (localValid) {
            for (const auto entity : state.entities) {
                const bool isLocal = entity == localCharacter;
                const auto metadataEntry = state.metadataCache.find(entity);
                const CharacterVisualMetadata* metadata = metadataEntry != state.metadataCache.end()
                    ? &metadataEntry->second : nullptr;
                if (isLocal && !Config::renderLocalPlayer) {
                    ++localSkipped;
                    if (iteration == 0)
                        DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=local_hidden\n",
                            static_cast<unsigned long long>(entity));
                    continue;
                }
                if (metadata && metadata->healthValid && metadata->dead) {
                    ++deadSkipped;
                    if (iteration == 0)
                        DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=dead hp=%.3f/%.3f\n",
                            static_cast<unsigned long long>(entity),
                            static_cast<double>(metadata->hp),
                            static_cast<double>(metadata->maxHp));
                    continue;
                }
                std::uint64_t positionField{};
                Vector3 position{};
                if (!AddAddress(entity, Config::EntityWorldPosition, positionField)
                    || !UserAddress(positionField) || !reader.Read(positionField, position)
                    || !Finite(position) || !NonZero(position)) {
                    ++positionSkipped;
                    if (iteration == 0)
                        DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=position_invalid\n",
                            static_cast<unsigned long long>(entity));
                    continue;
                }
                const double distance = Distance(position, localPosition);
                if (!std::isfinite(distance) || distance > Config::CharacterDistanceLimit) {
                    ++distanceSkipped;
                    if (iteration == 0)
                        DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=distance "
                            "distance=%.3f position=(%.3f,%.3f,%.3f)\n",
                            static_cast<unsigned long long>(entity), distance,
                            static_cast<double>(position.x), static_cast<double>(position.y),
                            static_cast<double>(position.z));
                    continue;
                }
                ScreenCharacter marker{};
                if (!ProjectCharacterGeometry(position, camera, viewport.right,
                                              viewport.bottom, marker)) {
                    ++projectionSkipped;
                    if (iteration == 0) {
                        Vector3 head = position;
                        head.y += Config::CharacterHeight;
                        float feetX{}, feetY{}, headX{}, headY{};
                        const bool feetOk = WorldToScreen(position, camera, viewport.right,
                                                          viewport.bottom, feetX, feetY);
                        const bool headOk = WorldToScreen(head, camera, viewport.right,
                                                         viewport.bottom, headX, headY);
                        DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=projection "
                            "distance=%.3f feet=%d head=%d order=%d\n",
                            static_cast<unsigned long long>(entity), distance, feetOk, headOk,
                            feetOk && headOk && feetY > headY);
                    }
                    continue;
                }
                ++renderable;
                if (iteration == 0)
                    DiagnosticLog("[frame-diagnostic-entity] entity=0x%llX result=render "
                        "distance=%.3f screen=(%.1f,%.1f)-(%.1f,%.1f)\n",
                        static_cast<unsigned long long>(entity), distance,
                        static_cast<double>(marker.headX), static_cast<double>(marker.headY),
                        static_cast<double>(marker.feetX), static_cast<double>(marker.feetY));
            }
        }
        DiagnosticLog("[frame-diagnostic] iteration=%llu scan=%s rejection=%s cache=%zu "
            "viewport=%d camera=%d local=%d local_skip=%zu dead=%zu position=%zu "
            "distance=%zu projection=%zu renderable=%zu\n",
            static_cast<unsigned long long>(iteration), scanAccepted ? "accepted" : "rejected",
            scan.rejection, state.entities.size(), viewportValid, cameraValid, localValid,
            localSkipped, deadSkipped, positionSkipped, distanceSkipped, projectionSkipped,
            renderable);
        Sleep(Config::EntityScanIntervalMs);
    }
    DiagnosticLog("[frame-diagnostic] end\n");
}

struct Frame {
    bool visible = false;
    bool cameraValid = false;
    bool localAnchorValid = false;
    float x = 0, y = 0;
    Camera camera{};
    Vector3 localAnchor{};
    std::size_t cachedCharacters = 0;
    std::vector<ScreenCharacter> characters;
    bool gmValid = false;
    bool gmLooking = false;
    bool gmScreenValid = false;
    bool gmDirectionScreenValid = false;
    ImVec2 gmScreen{}, gmDirectionScreen{};
    GmCameraSample gm{};
};
Frame g_frame; // Accessed exclusively on the UI thread.

COLORREF CharacterColor(const ScreenCharacter& marker) {
    if (marker.local) return RGB(0, 255, 255);
    if (marker.faction == "US") return RGB(70, 140, 255);
    if (marker.faction == "USSR") return RGB(255, 70, 70);
    if (marker.faction == "FIA") return RGB(70, 220, 90);
    if (marker.faction == "CIV") return RGB(255, 235, 110);
    return RGB(150, 150, 150);
}

COLORREF HealthColor(float ratio) {
    ratio = (std::max)(0.0f, (std::min)(ratio, 1.0f));
    if (ratio >= 0.5f)
        return RGB(BYTE(std::lround((1.0f - ratio) * 2.0f * 255.0f)), 255, 0);
    return RGB(255, BYTE(std::lround(ratio * 2.0f * 255.0f)), 0);
}

float DistanceFadeFactor(double distance, float maximumDistance) {
    if (!std::isfinite(distance) || maximumDistance <= 0.0f) return 1.0f;
    const float normalized = (std::max)(0.0f, (std::min)(float(distance / maximumDistance), 1.0f));
    return 1.0f - normalized * 0.55f;
}
ImU32 ImGuiColor(COLORREF color, float alpha = 1.0f) {
    const int a = int(std::lround((std::max)(0.0f, (std::min)(alpha, 1.0f)) * 255.0f));
    return IM_COL32(GetRValue(color), GetGValue(color), GetBValue(color), a);
}
void AddOutlinedText(ImDrawList* draw, ImVec2 position, ImU32 color, const char* text) {
    constexpr ImU32 outline = IM_COL32(0, 0, 0, 230);
    draw->AddText(ImVec2(position.x - 1, position.y), outline, text);
    draw->AddText(ImVec2(position.x + 1, position.y), outline, text);
    draw->AddText(ImVec2(position.x, position.y - 1), outline, text);
    draw->AddText(ImVec2(position.x, position.y + 1), outline, text);
    draw->AddText(position, color, text);
}

ImU32 FrameTimeColor(double milliseconds) {
    switch (ClassifyFrameTime(milliseconds)) {
    case FrameTimeBand::Green: return IM_COL32(80, 230, 120, 255);
    case FrameTimeBand::Yellow: return IM_COL32(255, 215, 80, 255);
    default: return IM_COL32(255, 90, 90, 255);
    }
}

void DrawDebugHud(ImDrawList* draw) {
    constexpr float left = 12.0f;
    constexpr float top = 12.0f;
    constexpr float width = 356.0f;
    constexpr float padding = 10.0f;
    ImFont* font = ImGui::GetFont(); // Dear ImGui's default embedded font is monospace.
    const float fontSize = ImGui::GetFontSize() * 0.82f;
    const float lineHeight = fontSize + 3.0f;
    const bool rejected = g_readStatistics.consecutiveRejectedScans != 0;
    const float textLines = rejected ? 17.0f : 16.0f;
    constexpr float barHeight = 8.0f;
    const float height = padding * 2.0f + textLines * lineHeight + 3.0f + barHeight + 7.0f;
    draw->AddRectFilled({left, top}, {left + width, top + height},
        IM_COL32(10, 13, 18, 205), 7.0f);
    draw->AddRect({left, top}, {left + width, top + height},
        IM_COL32(90, 105, 125, 150), 7.0f);

    float y = top + padding;
    const auto line = [&](ImU32 color, const char* text) {
        draw->AddText(font, fontSize, {left + padding, y}, color, text);
        y += lineHeight;
    };
    char text[192]{};
    constexpr ImU32 normal = IM_COL32(220, 226, 235, 255);
    constexpr ImU32 dim = IM_COL32(145, 155, 170, 255);
    constexpr ImU32 amber = IM_COL32(255, 185, 70, 255);
    sprintf_s(text, "FPS: %5.1f", g_readStatistics.smoothedFps);
    line(normal, text);
    sprintf_s(text, "reads/frame: %llu | failed: %llu",
        static_cast<unsigned long long>(g_readStatistics.frameReads),
        static_cast<unsigned long long>(g_readStatistics.frameReadFailures));
    line(normal, text);
    sprintf_s(text, "frame: %5.2f ms", g_readStatistics.frameDeltaMs);
    line(FrameTimeColor(g_readStatistics.frameDeltaMs), text);
    sprintf_s(text, "work:    %5.2f ms", g_readStatistics.workMs);
    line(dim, text);
    sprintf_s(text, "wait:    %5.2f ms", g_readStatistics.waitMs);
    line(dim, text);
    sprintf_s(text, "present: %5.2f ms", g_readStatistics.presentMs);
    line(dim, text);
    sprintf_s(text, "camera age: %.1fms", g_readStatistics.cameraAgeMs);
    line(dim, text);
    sprintf_s(text, "cache: %zu | rendered: %zu", g_frame.cachedCharacters,
        g_frame.characters.size());
    line(normal, text);
    const ULONGLONG now = GetTickCount64();
    const double scanAge = g_readStatistics.lastAcceptedScanTick
        ? double(now - g_readStatistics.lastAcceptedScanTick) / 1000.0 : 0.0;
    if (g_readStatistics.lastAcceptedScanTick) sprintf_s(text, "scan: %.1fs ago", scanAge);
    else strcpy_s(text, "scan: waiting");
    line(normal, text);
    if (rejected) {
        sprintf_s(text, "scan rejected x%llu",
            static_cast<unsigned long long>(g_readStatistics.consecutiveRejectedScans));
        line(amber, text);
    }
    sprintf_s(text, "world entities: %u%s", g_readStatistics.lastWorldCount,
        g_readStatistics.lastScanPartial ? "  [PARTIAL]" : "");
    line(g_readStatistics.lastScanPartial ? amber : normal, text);
    sprintf_s(text, "full scans: %llu",
        static_cast<unsigned long long>(g_readStatistics.consecutiveFullScans));
    line(dim, text);
    if (g_frame.cameraValid) {
        sprintf_s(text, "camera: %.1f %.1f %.1f", double(g_frame.camera.position.x),
            double(g_frame.camera.position.y), double(g_frame.camera.position.z));
        line(normal, text);
        sprintf_s(text, "zoom: %.4f | heading: %5.1f deg", double(g_frame.camera.zoomScale),
            CameraHeadingDegrees(g_frame.camera.forward));
        line(normal, text);
    } else {
        line(amber, "camera: unavailable");
        line(dim, "zoom: -- | heading: --");
    }
    if (g_frame.localAnchorValid) {
        sprintf_s(text, "local: %.1f %.1f %.1f | LOCAL RENDER: %s",
            double(g_frame.localAnchor.x), double(g_frame.localAnchor.y),
            double(g_frame.localAnchor.z), Config::renderLocalPlayer ? "ON" : "OFF");
    } else {
        sprintf_s(text, "local: unavailable | LOCAL RENDER: %s",
            Config::renderLocalPlayer ? "ON" : "OFF");
    }
    line(Config::renderLocalPlayer ? IM_COL32(80, 220, 210, 255) : dim, text);

    y += 3.0f;
    const FrameWindowSummary frameWindow = SummarizeFrameWindow(
        g_readStatistics.frameTimes.data(), g_readStatistics.frameTimeCount);
    const char* prefix = "max 60f: ";
    char maximum[48]{};
    char average[80]{};
    sprintf_s(maximum, "%.1f ms", frameWindow.maximum);
    sprintf_s(average, " -- avg 60f: %.1f ms", frameWindow.average);
    ImVec2 textPosition{left + padding, y};
    draw->AddText(font, fontSize, textPosition, dim, prefix);
    textPosition.x += font->CalcTextSizeA(fontSize,
        (std::numeric_limits<float>::max)(), 0.0f, prefix).x;
    draw->AddText(font, fontSize, textPosition,
        frameWindow.maximum > 16.7 ? IM_COL32(255, 90, 90, 255) : dim, maximum);
    textPosition.x += font->CalcTextSizeA(fontSize,
        (std::numeric_limits<float>::max)(), 0.0f, maximum).x;
    draw->AddText(font, fontSize, textPosition, dim, average);
    y += lineHeight;
    std::uint64_t readScale = (std::max)(std::uint64_t{1}, g_readStatistics.frameReads);
    for (std::size_t i = 0; i < g_readStatistics.readHistoryCount; ++i)
        readScale = (std::max)(readScale, g_readStatistics.readHistory[i]);
    sprintf_s(text, "read budget: %llu / %llu",
        static_cast<unsigned long long>(g_readStatistics.frameReads),
        static_cast<unsigned long long>(readScale));
    line(dim, text);
    const float barLeft = left + padding;
    const float barRight = left + width - padding;
    const float ratio = float(double(g_readStatistics.frameReads) / double(readScale));
    draw->AddRectFilled({barLeft, y}, {barRight, y + barHeight}, IM_COL32(45, 52, 64, 230), 3.0f);
    draw->AddRectFilled({barLeft, y}, {barLeft + (barRight - barLeft) * ratio, y + barHeight},
        IM_COL32(90, 180, 255, 230), 3.0f);
}

constexpr char SessionCredit[] = "https://github.com/KoreanLandGiant";
ImFont* g_watermarkFont = nullptr;

struct SessionLabelText {
    char uptime[64]{};
    char gameVersion[96]{};
};

SessionLabelText MakeSessionLabelText(ULONGLONG uptimeMinutes) {
    SessionLabelText result{};
    sprintf_s(result.uptime, "UPTIME  %02llu:%02llu",
        static_cast<unsigned long long>(uptimeMinutes / 60),
        static_cast<unsigned long long>(uptimeMinutes % 60));
    sprintf_s(result.gameVersion, "ARMA REFORGER  %s", Config::TargetGameVersion);
    return result;
}

void DrawTerminalText(ImDrawList* draw, ImFont* font, float fontSize,
                      ImVec2 position, const char* value) {
    constexpr ImU32 shadow = IM_COL32(0, 12, 0, 160);
    constexpr ImU32 glow = IM_COL32(48, 255, 76, 35);
    constexpr ImU32 foreground = IM_COL32(108, 238, 122, 230);
    draw->AddText(font, fontSize, {position.x + 1.0f, position.y + 1.0f}, shadow, value);
    draw->AddText(font, fontSize, {position.x - 1.0f, position.y}, glow, value);
    draw->AddText(font, fontSize, {position.x + 1.0f, position.y}, glow, value);
    draw->AddText(font, fontSize, position, foreground, value);
}

void DrawSessionLabels(ImDrawList* draw) {
    ImFont* font = g_watermarkFont ? g_watermarkFont : ImGui::GetFont();
    const float fontSize = g_watermarkFont ? 16.0f : ImGui::GetFontSize() * 1.08f;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const SessionLabelText labels = MakeSessionLabelText(
        g_readStatistics.displayedUptimeMinutes);
    constexpr float margin = 12.0f, gap = 2.0f;
    const auto measure = [&](const char* value) {
        return font->CalcTextSizeA(fontSize,
            (std::numeric_limits<float>::max)(), 0.0f, value);
    };
    const float lineHeight = fontSize + 2.0f;
    const float rightWidth = (std::max)(measure(labels.uptime).x,
        measure(labels.gameVersion).x);
    const ImVec2 left{margin, display.y - margin - lineHeight};
    const ImVec2 right{display.x - margin - rightWidth,
        display.y - margin - lineHeight * 2.0f - gap};
    DrawTerminalText(draw, font, fontSize, left, SessionCredit);
    DrawTerminalText(draw, font, fontSize, right, labels.uptime);
    DrawTerminalText(draw, font, fontSize,
        {right.x, right.y + lineHeight + gap}, labels.gameVersion);
}

void DrawCharacterMarker(ImDrawList* draw, const ScreenCharacter& marker) {
    if (!marker.visible) return;
    const float fade = Config::DistanceFade
        ? DistanceFadeFactor(marker.distance, Config::CharacterDistanceLimit) : 1.0f;
    const ImU32 color = ImGuiColor(CharacterColor(marker), fade);
    if (marker.skeletonValid) {
        constexpr std::array<std::array<std::size_t, 2>, 17> links{{
            {Pelvis, SpineLow}, {SpineLow, SpineMid}, {SpineMid, SpineHigh},
            {SpineHigh, Neck}, {Neck, Head},
            {SpineHigh, ShoulderA}, {ShoulderA, ElbowA}, {ElbowA, HandA},
            {SpineHigh, ShoulderB}, {ShoulderB, ElbowB}, {ElbowB, HandB},
            {Pelvis, HipA}, {HipA, KneeA}, {KneeA, FootA},
            {Pelvis, HipB}, {HipB, KneeB}, {KneeB, FootB}
        }};
        for (const auto& link : links)
            draw->AddLine(marker.skeleton[link[0]], marker.skeleton[link[1]], color, 1.5f);
    }
    const float height = marker.feetY - marker.headY;
    const float width = height * 0.5f;
    const float corner = (std::min)(width, height) * 0.25f;
    const float center = marker.feetX;
    const float bottom = marker.feetY;
    const float top = marker.headY;
    const float left = center - width * 0.5f;
    const float right = center + width * 0.5f;
    constexpr float thickness = 2.0f;
    if (Config::BoxMode == 1) {
    draw->AddLine({left, top + corner}, {left, top}, color, thickness);
    draw->AddLine({left, top}, {left + corner, top}, color, thickness);
    draw->AddLine({right - corner, top}, {right, top}, color, thickness);
    draw->AddLine({right, top}, {right, top + corner}, color, thickness);
    draw->AddLine({left, bottom - corner}, {left, bottom}, color, thickness);
    draw->AddLine({left, bottom}, {left + corner, bottom}, color, thickness);
    draw->AddLine({right - corner, bottom}, {right, bottom}, color, thickness);
    draw->AddLine({right, bottom}, {right, bottom - corner}, color, thickness);
    } else if (Config::BoxMode == 2) {
        draw->AddRect({left, top}, {right, bottom}, color, 0.0f, 0, thickness);
    }
    if (Config::ShowSnaplines) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        draw->AddLine({display.x * 0.5f, display.y}, {center, bottom}, color, 1.0f);
    }
    if (marker.local) {
        draw->AddLine({center - 6, bottom}, {center + 7, bottom}, color, thickness);
        draw->AddLine({center, bottom - 6}, {center, bottom + 7}, color, thickness);
    }

    const bool health = Config::ShowHealthBar && marker.healthValid && marker.maxHp > 0.0f;
    if (health) {
        const float ratio = (std::max)(0.0f, (std::min)(marker.hp / marker.maxHp, 1.0f));
        const ImU32 healthColor = ImGuiColor(HealthColor(ratio), fade);
        const ImU32 background = IM_COL32(45, 45, 45, int(std::lround(fade * 255.0f)));
        if (Config::VerticalHealthBar) {
            const float x = left - 5.0f;
            draw->AddLine({x, top}, {x, bottom}, background, 3.0f);
            draw->AddLine({x, bottom}, {x, bottom - height * ratio}, healthColor, 3.0f);
        } else {
            const float y = top - 4.0f;
            draw->AddLine({left, y}, {right + 1.0f, y}, background, 3.0f);
            draw->AddLine({left, y}, {left + (right - left) * ratio, y}, healthColor, 3.0f);
        }
    }

    if (Config::ShowDistanceText) {
        char distance[48]{};
        if (marker.local) sprintf_s(distance, "LOCAL  %.0f m", marker.distance);
        else sprintf_s(distance, "%.0f m", marker.distance);
        const ImVec2 size = ImGui::CalcTextSize(distance);
        AddOutlinedText(draw, {center - size.x * 0.5f, bottom + 4.0f}, color, distance);
    }
    const char* topLabel = Config::ShowPlayerNames && !marker.playerName.empty() ? marker.playerName.c_str()
        : (Config::ShowAiLabel ? "AI" : nullptr);
    if (topLabel) {
        const ImVec2 size = ImGui::CalcTextSize(topLabel);
        const float healthOffset = health && !Config::VerticalHealthBar ? 6.0f : 2.0f;
        AddOutlinedText(draw, {center - size.x * 0.5f, top - size.y - healthOffset},
                        color, topLabel);
    }
}

bool g_menuOpen = false;
bool g_imguiReady = false;
HWND g_gameWindow = nullptr;

void SetMenuInteraction(HWND overlay, bool open) {
    g_menuOpen = open;
    LONG_PTR style = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
    if (open) {
        // A layered DirectComposition surface can remain transparent to hit
        // testing even after WS_EX_TRANSPARENT is cleared. Menu mode therefore
        // becomes an ordinary rectangular input window until it is closed.
        style &= ~static_cast<LONG_PTR>(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        style |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }
    SetWindowLongPtrW(overlay, GWL_EXSTYLE, style);
    SetWindowPos(overlay, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED
                 | (open ? SWP_SHOWWINDOW : SWP_NOACTIVATE));
    if (open) {
        SetForegroundWindow(overlay);
        SetActiveWindow(overlay);
        SetFocus(overlay);
        SetCapture(overlay);
    } else if (IsWindow(g_gameWindow)) {
        if (GetCapture() == overlay) ReleaseCapture();
        SetForegroundWindow(g_gameWindow);
    } else if (GetCapture() == overlay) {
        ReleaseCapture();
    }
}

enum class FrameWaitResult { Ready, Messages, Failed };

class OverlayRenderer {
public:
    bool Initialize(HWND window) {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL selected{};
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, static_cast<UINT>(_countof(levels)), D3D11_SDK_VERSION,
            device_.GetAddressOf(), &selected, context_.GetAddressOf());
        if (hr == E_INVALIDARG) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                levels + 1, 1, D3D11_SDK_VERSION, device_.GetAddressOf(), &selected,
                context_.GetAddressOf());
        }
        if (FAILED(hr)) return false;

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
        if (FAILED(device_.As(&dxgiDevice))
            || FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))
            || FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) return false;

        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width = 1;
        description.Height = 1;
        description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = 2;
        description.Scaling = DXGI_SCALING_STRETCH;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        description.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        hr = factory->CreateSwapChainForComposition(device_.Get(), &description, nullptr,
                                                     swapChain_.GetAddressOf());
        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IDXGISwapChain2> waitableSwapChain;
            if (SUCCEEDED(swapChain_.As(&waitableSwapChain))
                && SUCCEEDED(waitableSwapChain->SetMaximumFrameLatency(1))) {
                frameLatencyWaitableObject_ = waitableSwapChain->GetFrameLatencyWaitableObject();
                if (frameLatencyWaitableObject_) latencySwapChain_ = std::move(waitableSwapChain);
            }
        }
        if (!frameLatencyWaitableObject_) {
            // Some DirectComposition stacks reject the waitable-object flag. Recreate
            // without it, but still force a one-frame queue through DXGI.
            latencySwapChain_.Reset();
            swapChain_.Reset();
            description.Flags = 0;
            if (FAILED(factory->CreateSwapChainForComposition(device_.Get(), &description, nullptr,
                                                               swapChain_.GetAddressOf()))) return false;
            if (FAILED(swapChain_.As(&latencySwapChain_))
                || FAILED(latencySwapChain_->SetMaximumFrameLatency(1))) {
                Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice1;
                if (FAILED(dxgiDevice.As(&dxgiDevice1))
                    || FAILED(dxgiDevice1->SetMaximumFrameLatency(1))) return false;
                latencySwapChain_.Reset();
            }
        }
        swapChainFlags_ = description.Flags;
        DiagnosticLog("[renderer] frame_latency=%s maximum=1 sync_interval=1\n",
            frameLatencyWaitableObject_ ? "waitable_object" : "dxgi_fallback");
        if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(composition_.GetAddressOf())))
            || FAILED(composition_->CreateTargetForHwnd(window, TRUE, target_.GetAddressOf()))
            || FAILED(composition_->CreateVisual(visual_.GetAddressOf()))
            || FAILED(visual_->SetContent(swapChain_.Get()))
            || FAILED(target_->SetRoot(visual_.Get()))
            || FAILED(composition_->Commit())
            || !CreateRenderTarget()) return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImFont* defaultFont = ImGui::GetIO().Fonts->AddFontDefault();
        ImGui::GetIO().FontDefault = defaultFont;
        char windowsDirectory[MAX_PATH]{};
        const UINT windowsLength = GetWindowsDirectoryA(
            windowsDirectory, static_cast<UINT>(_countof(windowsDirectory)));
        if (windowsLength && windowsLength < _countof(windowsDirectory)) {
            char fontPath[MAX_PATH * 2]{};
            sprintf_s(fontPath, "%s\\Fonts\\consolab.ttf", windowsDirectory);
            g_watermarkFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, 16.0f);
            if (!g_watermarkFont) {
                sprintf_s(fontPath, "%s\\Fonts\\lucon.ttf", windowsDirectory);
                g_watermarkFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, 16.0f);
            }
        }
        ImGui::StyleColorsDark();
        if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX11_Init(device_.Get(), context_.Get())) {
            Shutdown();
            return false;
        }
        g_imguiReady = true;
        return true;
    }

    void Shutdown() {
        g_imguiReady = false;
        g_watermarkFont = nullptr;
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        renderTarget_.Reset();
        frameLatencyWaitableObject_ = nullptr;
        latencySwapChain_.Reset();
        visual_.Reset(); target_.Reset(); composition_.Reset(); swapChain_.Reset();
        context_.Reset(); device_.Reset();
        width_ = height_ = 0;
    }

    bool Resize(UINT width, UINT height) {
        if (!width || !height) return false;
        if (width == width_ && height == height_) return true;
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        renderTarget_.Reset();
        if (FAILED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                             swapChainFlags_))) return false;
        width_ = width; height_ = height;
        return CreateRenderTarget();
    }

    FrameWaitResult WaitForFrame() const {
        if (!frameLatencyWaitableObject_) return FrameWaitResult::Ready;
        const DWORD result = MsgWaitForMultipleObjectsEx(1, &frameLatencyWaitableObject_, INFINITE,
            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_OBJECT_0) return FrameWaitResult::Ready;
        if (result == WAIT_OBJECT_0 + 1) return FrameWaitResult::Messages;
        return FrameWaitResult::Failed;
    }

    bool RenderFrame() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::GetIO().MouseDrawCursor = g_menuOpen;
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (g_frame.visible) {
            constexpr ImU32 headColor = IM_COL32(0, 255, 80, 255);
            draw->AddLine({g_frame.x - 8, g_frame.y}, {g_frame.x + 9, g_frame.y}, headColor, 2.0f);
            draw->AddLine({g_frame.x, g_frame.y - 8}, {g_frame.x, g_frame.y + 9}, headColor, 2.0f);
        }
        for (const auto& character : g_frame.characters) DrawCharacterMarker(draw, character);
        if (g_frame.gmValid) {
            if (g_frame.gmScreenValid) {
                constexpr ImU32 markerColor = IM_COL32(255, 195, 70, 255);
                draw->AddCircle(g_frame.gmScreen, 7.0f, markerColor, 16, 2.0f);
                if (g_frame.gmDirectionScreenValid)
                    draw->AddLine(g_frame.gmScreen, g_frame.gmDirectionScreen,
                                  markerColor, 2.0f);
            }
            const float right = ImGui::GetIO().DisplaySize.x - 52.0f;
            constexpr float top = 56.0f;
            const ImVec2 origin{right, top};
            draw->AddCircle(origin, 24.0f, IM_COL32(255, 195, 70, 220), 24, 1.5f);
            const float planar = std::sqrt(g_frame.gm.forward.x*g_frame.gm.forward.x
                + g_frame.gm.forward.z*g_frame.gm.forward.z);
            if (planar > 0.001f) {
                const ImVec2 tip{right + 20.0f*g_frame.gm.forward.x/planar,
                    top - 20.0f*g_frame.gm.forward.z/planar};
                draw->AddLine(origin, tip, IM_COL32(255, 195, 70, 255), 3.0f);
            }
            draw->AddText({right - 28.0f, top + 28.0f},
                IM_COL32(255, 195, 70, 255), g_frame.gmLooking ? "GM LOOKING" : "GM CAMERA");
            char gmPosition[96]{};
            sprintf_s(gmPosition, "%.0f  %.0f  %.0f", double(g_frame.gm.position.x),
                double(g_frame.gm.position.y), double(g_frame.gm.position.z));
            draw->AddText({right - 90.0f, top + 45.0f},
                IM_COL32(255, 195, 70, 255), gmPosition);
        }
        if (Config::ShowDebugHud) {
            DrawDebugHud(draw);
        }
        DrawSessionLabels(draw);
        if (g_menuOpen) {
            ImGui::SetNextWindowBgAlpha(0.94f);
            ImGui::Begin("Overlay settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::TextDisabled("Public preview: data source not connected");
            ImGui::TextUnformatted("Insert toggles this menu");
            ImGui::Separator();
            ImGui::Checkbox("Health bars", &Config::ShowHealthBar);
            ImGui::Checkbox("Distance text", &Config::ShowDistanceText);
            ImGui::Checkbox("AI labels", &Config::ShowAiLabel);
            ImGui::Checkbox("Render local player", &Config::renderLocalPlayer);
            ImGui::SliderFloat("Maximum distance", &Config::CharacterDistanceLimit,
                               10.0f, 2000.0f, "%.0f m");
            ImGui::Separator();
            ImGui::Checkbox("Distance color fade", &Config::DistanceFade);
            ImGui::Checkbox("Vertical health bars", &Config::VerticalHealthBar);
            ImGui::Checkbox("Show debug HUD", &Config::ShowDebugHud);
            ImGui::Checkbox("Skeleton", &Config::ShowSkeleton);
            ImGui::Checkbox("GM camera", &Config::ShowGmCamera);
            ImGui::Checkbox("Snaplines", &Config::ShowSnaplines);
            ImGui::Checkbox("Player names", &Config::ShowPlayerNames);
            ImGui::Checkbox("Alive", &Config::ShowAlive);
            ImGui::Checkbox("Dead", &Config::ShowDead);
            ImGui::Checkbox("US", &Config::ShowUS);
            ImGui::SameLine(); ImGui::Checkbox("USSR", &Config::ShowUSSR);
            ImGui::Checkbox("FIA", &Config::ShowFIA);
            ImGui::SameLine(); ImGui::Checkbox("CIV", &Config::ShowCIV);
            constexpr const char* boxModes[] = {"Off", "Corner", "2D"};
            ImGui::Combo("Box mode", &Config::BoxMode, boxModes, IM_ARRAYSIZE(boxModes));
            ImGui::SeparatorText("Read statistics");
            ImGui::Text("Frame reads: %llu, failed: %llu (%.3f ms)",
                static_cast<unsigned long long>(g_readStatistics.frameReads),
                static_cast<unsigned long long>(g_readStatistics.frameReadFailures),
                g_readStatistics.frameReadMs);
            ImGui::Text("Scan reads: %llu, failed: %llu (%.3f ms)",
                static_cast<unsigned long long>(g_readStatistics.scanReads),
                static_cast<unsigned long long>(g_readStatistics.scanReadFailures),
                g_readStatistics.scanReadMs);
            ImGui::Text("Scan: %.3f ms", g_readStatistics.scanMs);
            ImGui::Text("Loop: %.3f ms", g_readStatistics.iterationMs);
            ImGui::End();
        }
        ImGui::Render();
        const float clear[4] = {0, 0, 0, 0};
        ID3D11RenderTargetView* target = renderTarget_.Get();
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->ClearRenderTargetView(target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        MarkFrameWorkComplete();
        LARGE_INTEGER presentStart{}, presentEnd{};
        QueryPerformanceCounter(&presentStart);
        const HRESULT presented = swapChain_->Present(1, 0);
        QueryPerformanceCounter(&presentEnd);
        RecordPresentTime(presentStart, presentEnd);
        RecordCameraAge(presentEnd);
        return SUCCEEDED(presented);
    }

private:
    bool CreateRenderTarget() {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        return SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf())))
            && SUCCEEDED(device_->CreateRenderTargetView(buffer.Get(), nullptr,
                                                         renderTarget_.GetAddressOf()));
    }
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> latencySwapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> visual_;
    HANDLE frameLatencyWaitableObject_ = nullptr; // Owned by the swap chain; do not CloseHandle.
    UINT swapChainFlags_ = 0;
    UINT width_ = 0, height_ = 0;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (g_imguiReady && ImGui_ImplWin32_WndProcHandler(hwnd, message, w, l)) return TRUE;
    switch (message) {
    case WM_MOUSEACTIVATE:
        return g_menuOpen ? MA_ACTIVATE : MA_NOACTIVATE;
    case WM_HOTKEY:
        if (w == 1) {
            SetMenuInteraction(hwnd, !g_menuOpen);
            return 0;
        }
        break;
    case WM_NCHITTEST: return g_menuOpen ? HTCLIENT : HTTRANSPARENT;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

struct WindowSearch { DWORD pid; HWND result; };
BOOL CALLBACK FindWindowCallback(HWND hwnd, LPARAM param) {
    auto& search = *reinterpret_cast<WindowSearch*>(param);
    DWORD pid{}; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == search.pid && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
        search.result = hwnd; return FALSE;
    }
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    const std::wstring configPath = ConfigFilePath();
    ApplyConfig(LoadConfigFile(configPath.c_str(), CurrentConfig()));
    const DWORD pid = FindProcess(Config::ProcessName);
    if (!pid) {
        MessageBoxW(nullptr, L"Target game window was not found.",
                    Config::ProcessName, MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ArmAHelperPublicPreview";
    if (!RegisterClassW(&wc)) return 1;
    HWND overlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED
        | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        wc.lpszClassName, L"ArmA Helper", WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!overlay) { UnregisterClassW(wc.lpszClassName, instance); return 1; }

    OverlayRenderer renderer;
    if (!renderer.Initialize(overlay)) {
        DestroyWindow(overlay);
        UnregisterClassW(wc.lpszClassName, instance);
        return 1;
    }
    if (!RegisterHotKey(overlay, 1, MOD_NOREPEAT, VK_INSERT)) {
        renderer.Shutdown();
        DestroyWindow(overlay);
        UnregisterClassW(wc.lpszClassName, instance);
        return 1;
    }

    InitializeReadStatistics();
    bool running = true;
    while (running) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || (GetAsyncKeyState(VK_END) & 0x8000)
            || FindProcess(Config::ProcessName) != pid) break;

        WindowSearch search{pid, nullptr};
        EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&search));
        RECT viewport{};
        POINT origin{};
        g_gameWindow = search.result;
        const HWND foreground = GetForegroundWindow();
        const bool overlayActive = g_menuOpen && foreground == overlay;
        if (!search.result || IsIconic(search.result)
            || (foreground != search.result && !overlayActive)
            || !GetClientRect(search.result, &viewport)
            || !ClientToScreen(search.result, &origin)
            || viewport.right <= 0 || viewport.bottom <= 0) {
            ShowWindow(overlay, SW_HIDE);
            MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            continue;
        }
        SetWindowPos(overlay, HWND_TOPMOST, origin.x, origin.y,
                     viewport.right, viewport.bottom,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (!renderer.Resize(static_cast<UINT>(viewport.right),
                             static_cast<UINT>(viewport.bottom))) break;

        const FrameWaitResult waitResult = renderer.WaitForFrame();
        if (waitResult == FrameWaitResult::Messages) continue;
        if (waitResult == FrameWaitResult::Failed) break;

        BeginFrameStatistics();
        // No live acquisition is shipped in this public preview. Keep the
        // current frame empty rather than reusing any previous visualization.
        g_frame.visible = false;
        g_frame.cameraValid = false;
        g_frame.localAnchorValid = false;
        g_frame.cachedCharacters = 0;
        g_frame.characters.clear();
        g_frame.gm = {};
        g_frame.gmValid = false;
        g_frame.gmLooking = false;
        g_frame.gmScreenValid = false;
        g_frame.gmDirectionScreenValid = false;
        if (!renderer.RenderFrame()) break;
        EndFrameStatistics();
    }
    SaveConfigFile(configPath.c_str(), CurrentConfig());
    UnregisterHotKey(overlay, 1);
    renderer.Shutdown();
    if (IsWindow(overlay)) DestroyWindow(overlay);
    UnregisterClassW(wc.lpszClassName, instance);
    return 0;
}