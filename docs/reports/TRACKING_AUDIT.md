# Phase 0 tracking, runtime and build audit

Audit date: 2026-11-15 UTC. Source references in this report refer to the **unchanged baseline** `handbridge-v2` commit `5f547181f6d512ab6a8d2146022227e26cec48d2`, whose C++ application matches `master` commit `054e578525a5d5202ea8f6eb417ce55ded7d61b8`. New implementation results belong in CURRENT_STATE and SESSION_HANDOFF; this report records baseline evidence.

Method: read the application, build definitions, existing tests and project instructions; retrieve official API documentation over HTTPS. No original hardware application was compiled or run during this audit. No LeapC SDK header/library was found in the checkout, `/usr/include` or `/opt`. **UNTESTED ON HARDWARE** throughout.

## Actual executable path and ownership

1. `src/main.cpp:149` constructs `AppCore` with a required `MainAppWindow&`; initialization of SDL/OpenGL/ImGui must succeed before `AppCore::start` (`main.cpp:275`, `:314`). A hidden window/tray process is still a GUI application, not an independent headless engine.
2. `AppCore` owns `LeapConnection`, `LeapInput`, `LeapSorter`, `DataProcessor` and `OscSender` (`src/app/AppCore.hpp:63`). `LeapConnection` creates and opens the LeapC connection with `eLeapConnectionConfig_MultiDeviceAware` (`src/pipeline/00_LeapConnection.cpp:9`). Its destructor closes/destroys the connection. Connection ownership outlives `LeapInput` in declaration/destruction order.
3. `LeapInput::start` enumerates devices synchronously, installs a frame-copy callback, then starts **one** poll thread (`src/core/LeapInput.cpp:39`). Multiple camera events share that thread. `LeapPoller::poll` calls `LeapPollConnection` once with a 30 ms timeout, identifies tracking source using `msg.device_id`, converts/copies SDK values to owned `FrameData`, then invokes the callback (`src/pipeline/01_LeapPoller.cpp:296`, `:321`). This source-ID routing and owned copy are useful preserved behavior.
4. `LeapInput` pushes to a shared SPSC queue with configured application capacity 256 (`src/app/AppCore.cpp:29`, `src/core/LeapInput.cpp:58`). There is one producer and one consumer in the observed path, so SPSC topology currently fits. Overflow drops the incoming frame; it does not replace an older frame.
5. **Only the SDL main thread** calls `AppCore::processPendingFrames`, which drains the queue, calls sorter, processor, then synchronous OSC send (`src/main.cpp:349`, `:369`; `src/app/AppCore.cpp:321`). The processor's UI callback is also synchronous (`AppCore.cpp:122`). Rendering and Windows modal/menu work therefore share the output's consumer thread.
6. The window can be hidden, but closing the process/event loop stops tracking (`main.cpp:355`, `:396`). No separately attachable GUI process, IPC, independent hardware headless host, or automatic service supervisor exists at baseline.

## Confirmed baseline defects and incomplete behavior

| Priority | Evidence | Established behavior / consequence |
| --- | --- | --- |
| P0 | `main.cpp:349`, `:369`, `:386`, `:484` | SDL event handling, queue consumption, rendering and `TrackPopupMenu` share a thread. A blocked UI/menu can pause OSC while the poll worker keeps filling its queue. A code-level starvation route is established; the owner's field incident has **not** been reproduced. |
| P0 | `01_LeapPoller.cpp:199`–`:228` | On a new-device subscription failure, code logs but continues adding the device and notifying connected. There is no retry schedule or separate subscribed state, so a visible connected camera can have no tracking subscription. |
| P0 | `01_LeapPoller.cpp:364`; `AppCore.cpp:225` | Connection loss only invokes an optional notification. AppCore registers device callbacks, but never registers LeapInput service connect/disconnect callbacks. There is no application recovery state machine, device cleanup/re-enumeration on service loss, capped retry, or stale-frame watchdog. SDK's own reconnection behavior is a separate unverified question. |
| P0 | `AppCore.cpp:309`; `03_DataProcessor.cpp:121` | Device loss only updates UI. Legacy hand zeroing requires a later frame with a missing hand; device/service disappearance produces no such frame and can leave the receiver holding the last pose. |
| P1 | `LeapInput.cpp:60`–`:64` | Queue overflow is visible only behind `VERBOSE_LEAP_LOGGING`; there are no unconditional stage/drop counters, queue-age metrics or stale-data rejection. |
| P1 | `AppCore.cpp:110`; `04_OscSender.cpp:92` | Every scalar produces AppLogger strings plus batch/value console logging and flushing; high-frequency hot-path work is unconditional. Cost is unmeasured and remains a plausible amplifier of backlog. |
| P1 | `04_OscSender.cpp:39`, `:73` | Socket initialization failure leaves null socket with no timed recovery. Send failures are logged without counters/backoff or supervision. `sendBundle` is a stub (`:63`); raw interface `send` always fails (`:28`). |
| P1 | `LeapInput.cpp:39`; official `LeapOpenConnection` docs | Initial enumeration occurs before the first poll. Official docs say connection establishment does not occur until first `LeapPollConnection`, so this enumeration is premature. Later device events may rescue startup; actual SDK startup results remain unverified. |
| P1 | `01_LeapPoller.cpp:137`, `LeapInput.cpp:39` | Repeated initialization clears the device list without closing old handles. `LeapInput::start` has no already-running guard; replacing a joinable `std::thread` terminates the process. AppCore guards its own start, but the input abstraction is not restart-safe independently. |
| P1 | `02_LeapSorter.cpp:10`, `:33`; `AppCore.cpp:301` | Assignment setter locks its map but reader does not. Runtime discovery applies saved hand filters from the poll thread while the main thread reads them: data race potential exists in actual topology. Lowercase saved assignments cannot match uppercased hand types. `NONE` means pass both hands, not disabled. |
| P1 | `MainAppWindow.cpp:499`, `:559`, `:835` | UI “snapshot” stores pointers to mutable device records after releasing mutex. `isConnected` and `assignedHand` are ordinary fields modified by device callbacks; reading them without locking can race. Atomic counters alone do not protect the full record. |
| P2 | `FrameData.hpp:7`; `HandData.hpp:55`; `01_LeapPoller.cpp:19` | Frames lack source frame ID and local monotonic arrival time; hands lack native SDK hand ID. Validity defaults true, with no adapter population of per-joint validity. Actual pinch/grab, confidence, source timestamp, raw palm/arm/finger/bone values are preserved. |
| P2 | `LeapInput.cpp:32`, `:93`, `:128` | `getNextFrame` returns the same cached frame indefinitely without consuming/resetting freshness; `emitTestFrame` is empty; a second poll loop after the first is effectively dead code. `highLevelCallback_` is stored but never invoked. |

### Proven defects versus incident hypotheses

The table establishes source behavior, not the incident's root cause. Three especially credible **unproven** incident hypotheses are (1) service/subscription loss with no application recovery, (2) blocked main/UI thread causing processing backlog, and (3) expensive per-scalar logging causing or worsening that backlog. USB power, Hyperion defects, UDP receiver failure, and driver issues have no confirming evidence in this cloud audit. Observe frame arrival, subscription result, enqueued/dropped/processed frames, send attempts/results, and receiver-side TouchDesigner data at the same incident before attributing cause.

Normal empty-hand frames must count as healthy frames. Their OSC silence after the one-time legacy zero transition is expected; count frames rather than hand poses or emitted packets to detect liveness.

## Preserve, repair, refactor, replace

| Component | Disposition | Rationale / migration boundary |
| --- | --- | --- |
| `FrameData`, `HandData`, OSC value DTOs | Preserve + extend | Existing values and raw mm semantics underpin compatibility; append frame/hand identity, arrival clock and validity rather than changing legacy values. |
| LeapC multi-device config, `msg.device_id` mapping, synchronous conversion | Preserve | Matches official multi-device API direction; avoid holding SDK pointers across polls. Keep SDK symbols isolated from mock tests. |
| `DataProcessor` address generation and loss handling | Preserve behind goldens; repair gaps incrementally | Reuse exact scalar addresses, casing, values and filters. Test additional device/service-loss invalidation separately. Do not silently add v2 or bundles. |
| `DeviceAliasManager`, config storage | Repair | Stable aliases are useful; serialization inconsistency and schema validation need regression coverage before migration. See CONFIG_SCHEMA and the compatibility audit. |
| `LeapSorter` | Repair / consolidate | One authoritative hand filter with documented normalization and disabled semantics; fix synchronization and preserve baseline NONE behavior until explicit migration. |
| `SpscQueue` | Preserve initially; refactor policy/metrics | Correct current producer topology, bounded capacity. Add unconditional counts and age/discard policy before changing queue technology. `noexcept` copy push can terminate on allocation failure for FrameData; generic exception guarantees need review. |
| `LeapInput` / `LeapPoller` ownership | Refactor | Introduce SDK-independent lifecycle contract and mock source, owner-thread handles, explicit subscription status, capped retry/backoff and shutdown/restart tests. Do not retrofit speculative SDK recovery without installed-header verification. |
| `OscSender` | Preserve scalar encoding; repair transport | Remove hot-path logging, expose truthful OS send/error metrics and recoverable failures. Keep legacy packet boundaries. |
| `AppCore` | Refactor | Retain known routing behavior while extracting a host independent of MainAppWindow, SDL and GUI lifecycle. Add optional telemetry sink, not required GUI callbacks. |
| ImGui UI | Preserve as rollback/reference; later replace controller | Separate Qt controller is specified for a later phase. Existing hand assignment, filter and OSC target controls are actually wired (`UIController.cpp:44`, `:122`, `:173`), so do not dismiss all controls as placeholders. |
| `HandleWrapper.hpp` | Preserve selectively | Existing move-only RAII primitive is useful; wrap connection and per-device cleanup with documented unsubscribe/close sequencing. |
| Duplicate / dead implementation paths | Exclude from new target; preserve source/history | `LeapConnectionImpl` is not AppCore's active connection; `LeapDeviceManager::poll` is empty; `MainAppWindow.hpp` is incomplete and not active `.h`; `DeviceTrackingModel` is tested independently but not used by runtime; `ThreadSafeQueue` is not active pipeline queue. The two global `IInputDevice` definitions have incompatible APIs and should not both enter a new target. |

## Build and existing-test findings

Baseline CMake is not a trustworthy build description:

- Requires C++17 and unconditionally loads SDK/UI/test dependencies; hard-coded `LEAP_SDK_PATH`/`OSCPACK_DIR` Windows paths, Win32-only oscpack files and OpenGL requirement prevent clean SDK-free operation.
- Core glob excludes `src/app/AppCore.cpp`; explicit UI list excludes `src/ui/UIController.cpp`, although executable needs both. It adds `src/core/` directory as a source. Main executable lacks `resource.rc`.
- Main executable includes abandoned `UIManager.cpp`, which has duplicate `shutdown()` definitions (`src/ui/UIManager.cpp:34`, `:96`), references nonexistent `Log::getInstance`, and includes a mismatched `UIManager.h` that declares another `MainAppWindow` instead of `UIManager`; its referenced members/class have no matching declaration. This file is not used by the actual WinMain path.
- `.vcxproj` includes AppCore/UIController and resource (`leapMotionApp.vcxproj:185`), unlike CMake, but imports unavailable sibling `../build/props` files and includes absolute developer/SDK paths (`:60`, `:72`, `:238`). SDK sources use hardcoded relative `../../LeapSDK/include/LeapC.h` despite configurable includes. VS and CMake source lists therefore disagree.
- 614 generated `build/` files are tracked at baseline; keep rollback history, use a clean separate output directory, and do not treat cached build products as a baseline pass.
- CMake registers only `test_DataProcessor` with `add_test`; other generated test executables are not registered. Catch2/AspectMapper is fetched but excluded from target loop; `TestLeapDataRouter.cpp` is excluded by lowercase glob.
- `tests/test_LeapPoller.cpp:5` tests its own `MockDeviceManager`, never production LeapPoller. It proves no SDK lifecycle or recovery behavior. `test_LeapConnection` only asserts a non-null opened handle, which is not service connectivity according to official asynchronous semantics.
- `test_PipelineEndToEnd.cpp:86` expects literal serials in addresses although production emits aliases `dev1`/`dev2`; this expectation is stale. It is not an actual UDP receiver test.

Retain useful existing processor, alias, config and sorter tests; isolate those without SDK/UI, fix stale assertions based on captured behavior, and register actual test executables. New deterministic lifecycle/fault tests must exercise production supervisor code, not reimplement desired behavior in test-only classes.

## Official API behavior checked

Retrieved successfully with TLS verification on 2026-11-15:

- [LeapC multi-device example](https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html): `eLeapConnectionConfig_MultiDeviceAware` in `LEAP_CONNECTION_CONFIG` opts into multi-device awareness; Device/DeviceLost events permit per-device subscribe/unsubscribe. Sample uses `msg.device_id` for tracking source identity and closes lost device handles. This supports preserving current direction, not assuming all existing lifecycle code is correct.
- [LeapC functions](https://docs.ultraleap.com/api-reference/tracking-api/group/group___functions.html): `LeapOpenConnection` is nonblocking; service establishment waits until first poll. `LeapSubscribeEvents` is required for device-specific events for multi-device-aware clients. `LeapPollConnection` event pointers remain valid only until the next poll or connection/device close, and concurrent polls return `eLeapRS_ConcurrentPoll`. Timeout returns `eLeapEventType_None`.
- Same functions reference: `LeapGetDeviceList` supports null-array count query followed by allocated array; device properties require open handle. `LeapGetDeviceInfo` serial retrieval supports null serial to discover length, then allocated buffer and a second call. Legacy `getDeviceSerial` adds an extra null-serial probe and assumes success on each sizing call; exact result handling must be checked against installed SDK/header/runtime before repair. This public text does **not** establish that a sizing call necessarily returns `eLeapRS_InsufficientBuffer`.
- [LeapC guide](https://docs.ultraleap.com/api-reference/tracking-api/leapc-guide.html): application must pump efficiently; LeapC drops messages if its internal queue fills. A fast, minimally blocking owner thread is therefore an API-level requirement, separate from Hand Bridge's own queue policy.

No proprietary SDK was downloaded or redistributed. Public docs do not establish the exact installed Hyperion ABI/version, idle frame cadence or service reconnection guarantee. Hardware adapter changes require exact local `LeapC.h`, SDK/service version record, a genuine Windows compile, and the two-camera physical checklist. Cloud mock recovery tests cannot satisfy those gates.

## Prioritized incremental implementation sequence

1. Establish C++20 SDK-free target and real registered tests; capture processor golden messages and config alias regression; preserve existing branches and original README. Record missing Windows SDK/dependencies explicitly.
2. Add minimal independently runnable mock host using existing processor/OSC semantics; two-camera synthetic frames, per-device monotonic liveness and transition counters; frame stall invalidates only that device and recovers deterministically when frames resume. Count empty frames as healthy.
3. Exercise repeated restart, stale frames, device/service loss, duplicate discovery, queue overflow and sender failure through the production headless foundation with injected time/faults. Keep no SDK symbol emulation in the test-only build.
4. Next bounded task after that foundation: implement the real LeapC adapter against installed Windows SDK, with RAII and explicit subscription/recovery state; carry metrics to Windows and reproduce incident. Gate UI work and optional protocol/processing features on reliability evidence.
