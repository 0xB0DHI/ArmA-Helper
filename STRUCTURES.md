# Controller architecture and integration map

> Historical research notes for the original working controller. This public
> copy retains the renderer and fake-memory sampling fixtures, but replaces
> live game-data acquisition with a placeholder. Paths, costs, and runtime
> claims below describe that original implementation, not the public preview.

## 2026-10-20 implementation status

The follow-up work order added persisted faction and alive/dead filters, box modes, snaplines, and player-name display controls in `controller.cpp`. Subsequent elevated read-only diagnostics supplied by the user established a 231-bone Chimera skeleton and the GM component's position/quaternion layout. Skeleton and GM samplers, frame scheduling, and draw paths are wired behind off-by-default toggles. The user confirmed bones render but reported lag on rapid view turns; a reduced read span and late camera reprojection await a second live check. A local inactive → active → inactive GM transition established an entity+0x100 flag changing 0 → 1 → 0; the indicator now requires a freshly rechecked active flag. Whether this same entity reflects another player's GM camera remains unverified in a multiplayer session. The MSVC `/W4 /WX` user build and deterministic controller suite pass with zero failures. The original context build below remains useful architectural background, but its "unimplemented" integration-map language is historical; the current map is in the section below.

Source-grounded against the current workspace on 2026-10-20. This document describes the read-only diagnostic overlay controller only. No controller, driver, ABI, or test source was changed while preparing it.

## Validated skeleton/GM integration (later 2026-10-20 work)

The user ran `controller-read-tests.exe --live-chain-diagnostic` in an elevated PowerShell while the GM camera and AI were moving. The complete 231-row entity-local translations for three characters and three GM component snapshots are present in the user-supplied diagnostic transcript in this chat. This file records the derived map and representative motion, not a second copy of all 693 rows. Normal character acquisition rejected `local_weak_failed` in GM mode; the diagnostic used a separate direct exact-class scan solely for validation. The production character scanner and its `SCR_ChimeraCharacterClass` exact filter were not broadened.

### Skeleton chain and joint map

The live chain is `entity+0x50 -> mesh component`; `mesh+0x30 -> matrix owner`; `owner+0x10 -> matrix array`, with 0x30-byte entries and translation at entry+0x24. The count chain is **separate**: `mesh+0x10 -> component data`; `data+0x30 -> MeshObject`; `MeshObject+0xD0 -> SkeletonContainer`; `SkeletonContainer+0x34 -> uint32 count`. All three sampled characters reported **231**. `mesh+0xD0` and `data+0xD0` are not the skeleton-container path; interpreting the former as a count produced garbage in an earlier diagnostic. The highest selected index is 201, so the sampler accepts counts 202..512 and never reads past the count. The selected joints fit two disjoint contiguous spans, 10..83 and 195..201: 74 × 48 = 3,552 bytes and 7 × 48 = 336 bytes (two `READ_MEMORY` calls, each below 4,096 bytes). Chain and count endpoints are rechecked before publication; any failure drops only that frame's skeleton.

| Joint | Index | Confidence/evidence |
|---|---:|---|
| Pelvis | 10 | high: waist level, between paired hip roots |
| Spine low / mid / high | 31 / 36 / 41 | high: rising central chain, approximately y=1.02/1.18/1.47 in upright sample |
| Neck / head | 81 / 83 | high: upper-midline; head 83 was already independently established |
| Shoulder / elbow / hand A | 45 / 48 / 51 | high: continuous outward/downward arm chain, varied across moving samples |
| Shoulder / elbow / hand B | 195 / 198 / 201 | high: paired arm on opposite side, varied across moving samples |
| Hip / knee / foot A | 18 / 19 / 23 | high: hip-to-ground chain with walking-leg variation |
| Hip / knee / foot B | 25 / 26 / 30 | high: opposite walking-leg chain |

The A/B labels refer to model sides, not screen left/right. One sample has arm A positions 45=(-0.110,1.427,-0.026), 48=(-0.259,1.188,-0.069), 51=(-0.286,0.931,0.010), while B is 195=(0.213,1.422,-0.141), 198=(0.335,1.172,-0.205), 201=(0.396,0.924,-0.117). Head 83 stayed near y=1.59. On another moving character, foot 23 shifted from approximately z=0.52 to z=-0.26 across samples while spine remained on the upper midline. This corroborates limb assignment, though no artist-authored bone-name table was available.

`FetchCharacterSkeleton` lives adjacent to `FetchLocalHeadBoneCandidate`; it uses fixed stack arrays, applies the entity transform, and publishes an entire `SkeletonWorldSample` only after validation. In `wWinMain`, it runs only for an exact-filtered character after the current position, local-player distance (≤500 m), and box projection succeed, immediately before appending `ScreenCharacter`. `ProjectSkeleton` requires every selected joint to project; `DrawCharacterMarker` draws line segments before box/text, with no target reads. A disabled toggle or character beyond 500 m causes **zero skeleton reads**. The adjacent mesh data/owner pointers are packed into one 40-byte read and one recheck. Successful sampling adds 15 transport reads per admitted projected character: seven chain/count/transform reads, two matrix-block reads, and six endpoint rechecks. Short-circuit failures cost less. If at least one skeleton was sampled, a second current-frame camera sample reprojects boxes and bones immediately before rendering to reduce camera-sample age during rapid view turns.

### GM camera chain and scheduling

The world list is `World+0x150` data and `World+0x15C` count, not `+0x158`: the same live sample showed +0x158=10239 and +0x15C=4598..4817. Exact class `SCR_EditorManagerEntityClass` was found. Component array and count are at GM entity+0x38/+0x44; observed count 20, preferred movement component at slot **18**. Slot 18's four contiguous floats at +0x68..+0x74 are quaternion `(x,y,z,w)`; `Vector3` position is at +0x80..+0x88. Across moving samples the position matched the entity transform translation at +0xA4 and standard quaternion rotation of +Z matched the entity transform forward basis at +0x98. One example: q=(0.249,0.108,-0.028,0.962), position=(6601.354,30.997,2308.575), forward≈(0.195,-0.486,0.852). The supplied +0x70 position, +0x90 quaternion, and +0x88 tick claims were falsified; no network-tick offset is currently established. Entity transform +0x80/+0x8C/+0x98 (right/up/forward) and +0xA4 translation are the verified fallback. Slot-18 class identity itself was inferred from its matching moving camera data; the alternative RTTI scan uses MSVC complete-object-locator normalization but was not needed or live-exercised in the observed session.

`DiscoverGmEntity` runs at one-second scan cadence, only while its toggle is on. It checks at most 256 world-list slots per tick, uses an exact GM class test distinct from the character filter, and caches the found list slot for membership recheck. `FetchGmCamera` runs at frame cadence immediately after the main `FetchCamera`. It first requires `GM entity+0x100 == 1`, an 8-byte value observed as 0 → 1 → 0 across an inactive → active → inactive local-GM transition; the value is rechecked after sampling, and inactive/transitioning samples publish nothing. It validates the observed quaternion/position against the current GM entity transform, falling back to that freshly read transform when snapshot data are invalid. The GM sample is cleared at frame start and only published after endpoint rechecks. The renderer consumes only current-frame GM fields to draw the camera's projected world marker/direction when visible, plus a view-direction compass, position text, and optional `GM LOOKING` label; the latter requires a valid current local-player anchor. In the observed GM mode, the game's local weak reference was absent, so LOOKING cannot be asserted there without a separate validated player-position source. A successful preferred-slot GM sample adds eleven transport reads per frame; an inactive flag costs only one read, and discovery adds bounded scan-phase reads. Camera remains the first read after the frame wait.

The user-facing toggle/config path remains `Config`, `PersistentConfig`, `CurrentConfig`, `ApplyConfig`, INI load/save, and the ImGui panel. Skeleton and GM are off by default. Renderer code performs no target reads. The existing always-attempted local bone-83 diagnostic remains an older, separate cost and was not changed.

## Source location and boundaries

- Controller implementation: `controller.cpp` (single translation unit; 2,790 lines).
- Live transport contract: omitted from this public copy.
- Deterministic controller tests: `controller-read-tests.cpp`, which includes `controller.cpp` and substitutes a `FakeReader` for the real transport.
- User-mode build entry point: `build-user.cmd`.

The controller never opens a process handle to the game. It identifies the game from the exact top-level window title `Arma Reforger`, opens `\\.\MyProjectLink`, resolves `ArmaReforgerSteam.exe` with `IOCTL_ENUM_MODULES`, and sends all target-memory reads through `IOCTL_READ_MEMORY`.

## Startup, overlay lifetime, and shutdown

`wWinMain` in `controller.cpp:2616` owns the complete lifecycle:

1. Enable process DPI awareness so viewport and overlay coordinates are physical pixels.
2. Locate `controller.ini` beside the executable, load the `[Overlay]` settings, clamp maximum distance to 10-2,000 m, and apply the settings to `Config`.
3. Find the game PID from the exact `Arma Reforger` window title. The formal process-name argument to `FindProcess` is not used.
4. Open `\\.\MyProjectLink` with `GENERIC_READ | GENERIC_WRITE`. The controller itself uses only the two established read-side IOCTLs.
5. Resolve the main module with `FindModuleBase`/`IOCTL_ENUM_MODULES`.
6. Dispatch any requested one-shot diagnostic (`--entity-class-diagnostic`, `--faction-diagnostic`, `--character-metadata-diagnostic`, `--scan-diagnostic`, or `--frame-diagnostic`) and exit, or continue into overlay mode.
7. Register the overlay window class, create a topmost transparent no-activate tool window, initialize D3D11, a premultiplied-alpha two-buffer flip-model composition swap chain, DirectComposition, and the Dear ImGui Win32/DX11 backends.
8. Register `Insert` as the menu hotkey and run the single-threaded UI/render loop until `End`, target loss/module-base change, renderer failure, or `WM_QUIT`.
9. Save `controller.ini`, unregister the hotkey, shut down ImGui/D3D/DirectComposition, destroy the window, and unregister the class. The RAII `Handle` closes the device on every exit path.

The swap chain requests `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` and maximum frame latency 1. If that combination is unavailable, initialization recreates the swap chain without the flag and still enforces maximum latency 1 through DXGI. Presentation is vsynced with `Present(1, 0)`.

`Insert` changes the overlay between click-through/no-activate mode and an interactive full-window menu. Interactive mode removes `WS_EX_LAYERED`, `WS_EX_TRANSPARENT`, and `WS_EX_NOACTIVATE`, captures the mouse, and activates the overlay. Closing the menu restores the styles and returns focus to the game.

## Main loop stage order

The loop in `wWinMain` (`controller.cpp:2665-2779`) has this exact order:

1. Pump queued Windows messages; handle `End`/quit.
2. Every 60 loop iterations, re-enumerate modules through the driver and terminate if the target module base changed or disappeared. This cadence is iteration-based, not wall-clock based.
3. Approximately every 1,000 ms, run `RefreshCharacterEntities` under the `Scan` statistics phase. An accepted scan atomically replaces the cached character list and metadata; a rejected scan leaves the previous accepted cache intact.
4. Rediscover and validate the visible game window/client rectangle. Hide the overlay and do no rendered-frame sampling while the game is minimized, missing, or not foreground (except when the settings menu owns focus).
5. Align/resize the overlay to the game client area.
6. Wait for frame-latency capacity or queued messages. A message wake returns to the message pump without sampling target memory.
7. Start frame statistics, then call `FetchCamera` immediately. This is the first target-memory operation of the rendered-frame phase.
8. Clear the UI-thread-owned `g_frame` publication object and publish the valid camera sample.
9. Call `FetchLocalHeadBoneCandidate`. This diagnostic bone-83 data path runs even when local-player rendering is disabled. If enabled and projectable, it sets the separate local cross in `g_frame`.
10. If the camera succeeded, call `FetchWorldLocalCharacter` to obtain the local character pointer and current local position used as the distance anchor.
11. Iterate the cached character pointers. In order: hide local when configured; drop known-dead entries; reread the current world position; validate finite/nonzero; apply maximum-distance culling; project feet and synthetic head points; attach scan-cadence metadata; append a `ScreenCharacter` to `g_frame.characters`.
12. `OverlayRenderer::RenderFrame` begins a new ImGui frame and emits draw passes in this order: local bone-83 cross, all character markers/labels/health bars, optional debug HUD, session watermarks, optional settings window. It then renders ImGui, presents the swap chain, and records presentation time/camera age. The 2026-10-20 watermark redesign keeps the GitHub URL at bottom-left and uptime/game version at bottom-right independently of the debug-HUD toggle; it removed the bottom read/failure/scan counters and build date. `DrawSessionLabels` uses a 16 px Consolas Bold font when available, green text with a soft shadow/glow and modest transparency but no backing panels; the menu and HUD retain the default ImGui font.
13. End frame statistics and repeat.

The renderer is a pure consumer of `g_frame`; it performs no game-memory reads. `WorldToScreen` and the current camera conventions are likewise outside the rendering backend and must remain unchanged unless separately revalidated.

## Entity acquisition and character filter

`RefreshCharacterEntities` (`controller.cpp:1583`) is the active one-second scanner and cache publisher.

It resolves root/game/world, reads the world entity-array header, resolves the world-local character weak reference, reads the local anchor, rejects counts above 81,920 or a configured wild count change, reads the entity-pointer array in 64-pointer blocks with scalar tail reads, validates and deduplicates pointers, builds the player-name map once, classifies uncached pointers, applies position/distance admission, refreshes metadata, rechecks the sampled roots/list/local weak reference, and only then publishes.

The active character filter is RTTI/class-name based, not a component-presence heuristic:

```text
entity + 0xE8 -> first
first  + 0x08 -> second
second + 0x10 -> string pointer
96-byte class text read
exact equality: "SCR_ChimeraCharacterClass"
```

This is implemented by `ReadClassName` (`controller.cpp:675`) and the exact comparison in `RefreshCharacterEntities` (`controller.cpp:1689`). `CharacterLike` is diagnostic-only fuzzy matching and is not the overlay admission filter. Skeletons therefore belong on the same `state.entities` set produced by the exact `SCR_ChimeraCharacterClass` comparison.

The class cache is keyed by entity pointer. An entity is reclassified only when its pointer is new; cache entries are removed when the pointer disappears from a successfully scanned world list. Faction, health, and player-name metadata are refreshed on every accepted scan. Cached metadata can be approximately one scan old, or older while scans are rejected. Current positions are never taken from the cache: every rendered-frame candidate gets a fresh position read, and failures suppress it for that frame.

Distance filtering exists twice deliberately:

- Scan admission: only exact-class characters with valid positions within `Config::CharacterDistanceLimit` of the current world-local position enter the published cache.
- Per-frame filtering: fresh positions are checked again against a fresh local anchor before projection and publication.

Known-dead characters are skipped per frame. Unknown health does not suppress a character. Local-character pointer equality controls local hiding. Projection rejects characters whose feet or synthetic `position + (0, 1.8, 0)` head are off-screen/behind the camera or produce invalid vertical ordering.

## Read transport and rules for adding a read path

The ABI is in `shared.h`:

- `IOCTL_READ_MEMORY`: function `0x800`, `METHOD_BUFFERED`, `FILE_ANY_ACCESS`.
- `IOCTL_ENUM_MODULES`: function `0x801`, `METHOD_BUFFERED`, `FILE_ANY_ACCESS`.
- `POINTER_READ_REQUEST`: 96 bytes, PID, base, up to 16 32-bit offsets, count, and an intentionally unused output pointer.
- One read payload is limited to 4,096 bytes by both the driver and `DriverReader::Read<T>`.

`DriverReader::Read<T>` (`controller.cpp:380`) is the only normal target-memory gateway. It requires a trivially copyable `T`, constructs a zero-initialized request, calls `DeviceIoControl` synchronously, requires exactly `sizeof(T)` returned, writes into a temporary, and publishes to the caller only after complete success. It also records attempts/failures and optional per-read QPC timing in the active `Frame` or `Scan` phase.

New controller read helpers should follow the existing testable pattern:

1. Implement a `template<class Reader>` helper so the exact chain can be exercised by `FakeReader` in `controller-read-tests.cpp`.
2. Put build-specific semantic offsets in `namespace Config`; compute every address with `AddAddress`, then check `UserAddress` before dereferencing.
3. Use only `reader.Read`; do not open the game process, call a write IOCTL, or bypass the statistics/validation path.
4. Accumulate into local temporary values/structures, validate all pointers, finite values, dimensions/counts, and arithmetic, and publish the output only when the whole sample is accepted.
5. For multi-read weak/lifetime-sensitive samples, use `ReadWeakSnapshot` and finish with the relevant root/pointer/`WeakUnchanged` rechecks. These reduce transition races but do not pin target objects.
6. On failure, publish neither partial bone sets nor fallback/stale positions. A failed optional skeleton sample should omit that skeleton for the frame; it need not invalidate an independently complete existing box sample.
7. Add deterministic success, invalid-hop, transition, and no-partial-publication tests before enabling the live path.

The driver supports offset-chained reads, but current controller code intentionally uses mostly explicit one-hop reads so it can validate and recheck each transition. Preserve that approach for weak references and mutable multi-read samples.

## Offset registry and conventions

The offset registry is only partially centralized. The primary controller constants are `constexpr` values in `namespace Config` at `controller.cpp:37-74`:

| Semantic name | Value | Meaning |
|---|---:|---|
| `GameBase` | `0x22CC978` | Module-relative root pointer slot RVA |
| `LocalPlayerController` | `0x310` | Game to local-controller weak field |
| `ControlledEntity` | `0x120` | Controller to controlled-entity weak field |
| `EntityWorldPosition` | `0xA4` | Embedded entity world position |
| `World` | `0x130` | Game to world pointer |
| `EntityArrayData` | `0x150` | World entity-list data pointer |
| `EntityArrayCount` | `0x15C` | World entity-list count |
| `WorldLocalCharacter` | `0x3A0` | World-local-character weak field |
| `EntityTransform` | `0x80` | Entity 3x4 world transform |
| `MeshObjectComponent` | `0x50` | Entity mesh component pointer |
| `HeadBoneIndexCandidate` | `83` | Existing diagnostic bone candidate |
| `BoneMatrixStride` | `0x30` | Packed `Matrix3x4` stride |

The convention is semantic PascalCase names, `std::uint64_t` for addresses/offsets, `std::uint32_t` for indices/counts, and comments describing provenance or limitations. Runtime settings also live in `Config`, but are `inline` mutable values rather than offsets.

Several established lower-level offsets remain inline literals in their owning helpers: class (`0xE8/0x08/0x10`), faction (`0x178/0x00/0x60`), health (`0x130/0x60/0x00/0x38`), player mapping (`0x2D8/0x18/0x24/0x48`), camera weak fields (`0x328/0x110`) and camera block (`0x80..0x194`), plus mesh/skeleton hops (`0x30/0x10/0x30/0xD0/0x10`). There is no separate offset header or versioned registry. New skeleton or GM-camera offsets should be added once to `Config` with semantic PascalCase names rather than introducing more duplicated literals; existing literals should not be broadly refactored as part of the feature unless separately scoped and regression-tested.

## Cost model and cadence

One `reader.Read` call equals one synchronous `IOCTL_READ_MEMORY` round trip. CPU-only validation, projection, ImGui drawing, and HUD rendering add no target reads. With `ShowDebugHud=true`, each read is timed with QPC; disabling the HUD removes that timing overhead but does not remove any target reads.

### Rendered-frame reads

On a fully successful normal frame:

| Path | Reads | Detail |
|---|---:|---|
| `FetchCamera` | 6 | root (1), manager weak (2), current-camera weak (2), packed 280-byte camera block (1) |
| `FetchLocalHeadBoneCandidate`, primary | 17 | full controller/entity/mesh/matrix/bone/transform sample plus root, weak, mesh, and matrix-base rechecks |
| Same, fallback mesh/skeleton chain | 20 | three more reads than the primary path |
| `FetchWorldLocalCharacter` | 9 | root/world/local weak/position plus root/world/local-weak rechecks |
| Per candidate after local/dead prefilters | 1 each | fresh `Vector3` position; distance and projection are CPU-only |

Therefore the successful steady-frame read count is:

```text
primary matrix path:  32 + R
fallback matrix path: 35 + R
```

`R` is the number of cached characters that survive the local-hidden and known-dead prefilters and therefore attempt a fresh position read. It includes candidates later rejected by invalid position, distance, or projection. Short-circuit failures reduce the actual count. If the camera fails, the local-anchor/per-entity stage is skipped, but the current local bone probe is still attempted.

The existing bone-83 probe is a large fixed per-frame cost even with `renderLocalPlayer=false`; that behavior is intentional current context, not a recommendation to copy it for all skeletons.

The main camera is explicitly prioritized: after frame capacity signals, `FetchCamera` is the first target-memory operation, and its fields are fetched in one packed read after resolving its two weak references. The local bone, local anchor, and entity positions come afterward. The one-second scanner and 60-iteration module-liveness check occur earlier in the outer loop, before the frame-capacity wait, and are not part of `frameReads`; a scan iteration can therefore add work before the next camera sample even though the camera remains first inside the rendered-frame phase.

### One-second scan reads

Let:

```text
B(n) = floor(n / 64) + (n mod 64)
```

because complete pointer blocks use one read and the remaining tail entries are read individually. For a successful scan, let `N` be raw world entries, `P` player-list entries, `U` unique entity pointers not already in the class cache, `M` exact character-class matches, and `C` exact-class characters admitted within range. The successful-path accounting is approximately:

```text
15 + B(N) + B(P) + player_entry_reads + 4*U + 1*M + 9*C
```

- The fixed 15 comprises root/world/list/local acquisition and final rechecks (12), plus player-manager/list/count acquisition (3).
- Each uncached class resolution is four reads: three pointer hops plus the 96-byte text block.
- Every exact-class match attempts one position read.
- Every in-range character then attempts up to four faction reads, four health reads (including the packed health pair), and one entity/player key read.
- A normally decodable player entry costs two reads (key plus inline name). Pointer fallback generally adds one more target read; a failed inline read followed by explicit pointer and text reads can take four attempts total for that player including the key.
- Invalid hops short-circuit and lower the count. Cached non-character pointers cost no classification reads on later accepted scans.

The scan runs at `EntityScanIntervalMs = 1000`. Faction, health, player names, class discovery, and broad entity-list acquisition are scan-cadence work. Camera, the current local anchor, accepted-character positions, and the existing local bone are frame-cadence work. The debug HUD exposes separate `reads/frame`, `scan reads`, failures, and timings rather than merging these budgets.

The module-liveness check uses `IOCTL_ENUM_MODULES`, not `IOCTL_READ_MEMORY`, approximately once per 60 loop iterations; it is not counted by `DriverReader` read statistics.

## Configuration and settings

Defaults are split between mutable `Config` values and the matching `PersistentConfig` struct at `controller.cpp:76`. `CurrentConfig`, `ApplyConfig`, `LoadConfigFile`, and `SaveConfigFile` map them to `[Overlay]` keys in `controller.ini` beside the executable.

Persisted controls are health bars, distance text, AI labels, local-player rendering, vertical health bars, distance fade, debug HUD, and maximum distance. The settings panel is built directly in `OverlayRenderer::RenderFrame` (`controller.cpp:2520-2544`). There is no general settings registry: every new persistent toggle requires coordinated additions to `Config`, `PersistentConfig`, `CurrentConfig`, `ApplyConfig`, INI load/save, and the ImGui panel. Rendering-only toggles must gate both read scheduling and drawing deliberately; otherwise an off toggle can still incur per-frame reads, as the current local bone path demonstrates.

## Integration map

All active controller attachments are in `controller.cpp`; deterministic coverage belongs in `controller-read-tests.cpp`.

### Skeleton reads

- Offset definitions: add semantic constants to `namespace Config` (`controller.cpp:37-74`). Keep `BoneMatrixStride`/`Matrix3x4` and the existing validated weak-reference helpers.
- Read helper: add a templated, all-or-nothing character skeleton sampler adjacent to `FetchLocalHeadBoneCandidate` (`controller.cpp:1818-1889`). It should accept an already admitted character pointer, validate the mesh/skeleton chain, read the requested contiguous matrix span in as few <=4,096-byte reads as the layout safely permits, validate every matrix, apply the entity transform, recheck mutable chain endpoints, and publish a complete temporary bone set only on success.
- Scheduling: in the per-entity loop at `controller.cpp:2748-2776`, preserve the existing local/dead/current-position/distance/projection order. The lowest-cost attachment is immediately after `ProjectCharacterGeometry` succeeds and before `g_frame.characters.push_back(marker)`. This keeps camera first, avoids skeleton reads for hidden/dead/out-of-range/off-screen entities, and does not reorder any existing stage.
- Frame publication: extend `ScreenCharacter` (`controller.cpp:1965`) with projected skeleton segments or a validated per-character skeleton payload. Never retain prior-frame bone positions.
- Tests: extend `controller-read-tests.cpp` with primary/fallback chain, arithmetic overflow, invalid matrix, transition/recheck, contiguous-block accounting, and no-partial-publication cases.

### Bone-line drawing

- Add a draw helper adjacent to `DrawCharacterMarker` (`controller.cpp:2290`), consuming only the current frame's already projected skeleton data.
- Invoke it from the existing character draw loop in `OverlayRenderer::RenderFrame` (`controller.cpp:2513`). Draw skeleton lines and the existing box in a defined order without any target reads in the renderer. A practical order is skeleton lines first and box/text/health afterward so labels remain legible.
- Reuse the character's faction/local color and distance fade unless a separately scoped visual design says otherwise.

### Game Master camera indicator

No GM-camera read path, offset, class, or current mode detector exists in the present source, so its memory chain must be validated before implementation; this document does not infer one.

- Offsets and type/state identifiers: add them to `Config` once validated.
- Read helper: place a templated `Fetch...GmCamera...` helper next to `ReadCameraBlock`/`FetchCamera` (`controller.cpp:1891-1938`) if it is camera-state based. If it is a world entity identified by class, discover/cache it in `RefreshCharacterEntities` using its own exact validated class filter rather than weakening the character filter.
- Scheduling: keep the main `FetchCamera` call first. Perform a GM-state/camera read immediately after that successful main-camera sample and before the existing local-bone work, or perform an entity-position read in the per-entity stage after its own current position/range checks. Do not replace or delay the projection camera sample.
- Publication/drawing: add explicit current-frame GM fields to `Frame` (`controller.cpp:2097`) and draw the indicator from `OverlayRenderer::RenderFrame` after world markers and before the HUD/settings panel. Clear the fields at the start of every frame so a failed read cannot render stale state.
- Tests: cover inactive/active states, invalid pointers, transition rechecks, projection rejection, and clearing after a failed frame.

### New toggles and filters

- Runtime/default values: `namespace Config` (`controller.cpp:37-74`).
- Persistence: `PersistentConfig`, `CurrentConfig`, `ApplyConfig`, `LoadConfigFile`, and `SaveConfigFile` (`controller.cpp:76-138`).
- UI: the `Overlay settings` block inside `OverlayRenderer::RenderFrame` (`controller.cpp:2520-2544`).
- Character-set filter: the exact class equality in `RefreshCharacterEntities` (`controller.cpp:1689`); do not use the diagnostic `CharacterLike` helper.
- Per-frame local/dead/range filters: the loop at `controller.cpp:2748-2763`.
- Read-cost gating: gate a new skeleton or GM read before calling its helper when its feature is disabled, unless continued collection while hidden is an explicit diagnostic requirement.

## Invariants to preserve during integration

- Existing read IOCTL numbers, structures, and semantics remain byte-for-byte unchanged.
- All target-memory reads stay behind `Reader::Read`/`DriverReader::Read`; the overlay remains read-only.
- Preserve `ReadWeakSnapshot`, `WeakUnchanged`, pointer/range/arithmetic checks, and failure-before-publication behavior.
- Keep `FetchCamera` first after the frame-latency wait and preserve the current packed camera block and `WorldToScreen` math.
- Never render a prior-frame entity or bone position after a current-frame read failure.
- Do not broaden the character class filter when enabling skeletons.
- Do not make the renderer perform target reads.
- Measure the new `reads/frame`, failure count, read time, camera age, and frame time with the existing HUD/statistics before and after enabling the new paths.
