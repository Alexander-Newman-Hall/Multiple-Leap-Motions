# Phase 0 code audit — 2026-11-15

Audited input: `handbridge-v2` at `5f547181f6d512ab6a8d2146022227e26cec48d2`.
Remote default `master` rechecked at `054e578525a5d5202ea8f6eb417ce55ded7d61b8`.
The extra branch commit adds the specification kit, not an engine implementation.
Work takes place on `codex/handbridge-phase0-reliability`; neither input branch is rewritten.
Line references below refer to the audited input commit unless stated otherwise.

## Actual executable path

The Visual Studio application enters `WinMain` (`src/main.cpp:88`), constructs a
`MainAppWindow` and `AppCore`, then drains frames in the SDL event/render loop
(`src/main.cpp:349-390`). `AppCore` owns `LeapConnection`, `LeapInput`, `LeapSorter`,
`DataProcessor`, and the `ITransportSink` implemented by `OscSender`
(`src/app/AppCore.hpp:62-98`, `src/app/AppCore.cpp:53-212`).

`LeapInput` creates one polling worker. The worker polls LeapC, copies SDK data
into `FrameData`, and pushes a shared SPSC queue. The main UI thread consumes
that queue (`src/core/LeapInput.cpp:39-77`, `src/app/AppCore.cpp:321-334`).
The sorter selects handedness; the processor emits raw millimeter scalar OSC;
oscpack serializes/sends one UDP datagram per scalar. Processing also calls the
UI directly. Hiding a tray window does not establish an independent headless
process; a blocked UI thread still blocks OSC processing.

## Component decisions

| Component | Decision | Evidence / reason |
|---|---|---|
| `HandData.hpp`, `FrameData.hpp` | Preserve and extend through an owned frame envelope | Already SDK independent; missing native hand/frame identity, arrival time and explicit validity domains. Preserve mm legacy values. |
| `03_DataProcessor.*` | Preserve with golden tests | Existing output is the compatibility authority. Quirks include loss-only finger `exists=0` and optional orientation/velocity/normal not cleared on loss. |
| `04_OscSender.*`, oscpack | Repair and preserve | Working scalar OSC implementation; noisy per-scalar logging, absent send metrics, raw-send stub and unimplemented bundles. |
| `ConfigManager.*`, `DeviceAliasManager.*` | Repair before migration | Confirmed uninitialized first-run settings and alias key mismatch; keep legacy keys/default port, introduce v2 schema later with explicit migration. |
| `02_LeapSorter.*` | Refactor behind tested headless selection | Uppercase-only matching conflicts with documented lowercase input; read path lacks assignment lock. `NONE` means pass-through in old code, not disabled. |
| `LeapInput.*`, pipeline `00/01` | Preserve as hardware reference; replace ownership/recovery incrementally | Explicit multi-device subscriptions and owned conversion are useful; service/subscription/freshness health and retry lifecycle incomplete. Installed SDK required before adapter changes. |
| `AppCore.*` | Refactor into headless runtime | Owns a UI reference and drains from UI loop; data path depends on rendering responsiveness. |
| `src/core/LeapConnectionImpl.*`, `LeapDeviceManager.*` | Retain as reference; retire after parity | Alternate wrappers are compiled but the actual application uses pipeline connection/poller; avoid maintaining two competing hardware adapters. |
| `src/ui/MainAppWindow.*`, `UIController.*`, renderer | Preserve legacy application; replace controller in Phase 3 | Actual ImGui UI remains reference. Qt process/IPC does not exist yet. Some controls have placeholder/disconnected behavior. |
| `UIManager.*`, `MainAppWindow.hpp`, duplicated ImGui backends | Do not include in new runtime | Dead/duplicate definitions; `UIManager.cpp` has duplicate shutdown and nonexistent logger calls. |
| DI/events and `AspectMapper` | Preserve only where used | Current AspectMapper is a stub, tests reference superseded APIs; do not make them domain dependencies. |
| CMake / VS project | Repair CMake; preserve VS as historical reference | CMake differs from actual VS source list, requires UI/SDK for all tests and omits JSON dependency. |
| Existing tests | Triage; keep working assertions, quarantine stale tests explicitly | Some tests call nonexistent APIs or test a local fake instead of production. Enumerate exclusions in build report. |
| Tracked `build/` | Preserve historical files for now; ignore new outputs | 614 tracked generated files of 739 total tracked files at input. Never build into that historical directory. |
| Original README | Preserve verbatim | Copy at `docs/legacy/README.md`; current README must distinguish shipped commands from plans. |

## Confirmed defects and diagnostic priority

1. **UI blocks output:** queue consumption is on the same thread as rendering and
   `TrackPopupMenu` (`src/main.cpp:369`, `:484`). This is a demonstrated coupling,
   not proof it caused the reported installation failure.
2. **Stale pose survives device loss:** `AppCore::handleDeviceLost` only notifies
   the UI (`src/app/AppCore.cpp:309-313`). No empty frame reaches the processor;
   pose channels can remain held at the receiver until another frame arrives.
3. **Failed subscription appears connected:** `LeapPoller.cpp:199-228` keeps a
   device after `LeapSubscribeEvents` fails. No retry state distinguishes it.
4. **Service loss has no supervised path:** poller notifies connection loss,
   while AppCore wires device callbacks but not service callbacks. Do not assume
   reopening the whole connection is always required by LeapC; verify exact SDK.
5. **First-run configuration is undefined:** constructor initializes gains only
   (`ConfigManager.cpp:35-39`); primitive port/flags remain indeterminate if load
   fails. Config load fallbacks and processor defaults establish repair defaults.
6. **Aliases do not round-trip:** serializer writes `deviceAliases`, save reads
   `device_aliases`, and loader expects a different wrapper (`DeviceAliasManager.cpp:47-79`,
   `ConfigManager.cpp:93-95`, `:159`). A saved null map cannot recover old aliases.
7. **Dropped frames are silent:** queue push failure is logged only under
   `VERBOSE_LEAP_LOGGING` (`LeapInput.cpp:59-65`), with no unconditional counter.
8. **Hot-path overhead:** every scalar logs in AppCore and sender
   (`AppCore.cpp:103-109`, `04_OscSender.cpp:108-114`). Impact requires measurement.

Unproven field hypotheses: USB/power interruption, Hyperion service failure,
poll error, queue starvation, sender failure, receiver/port mismatch, and UI
blocking. The reported real-camera silent OSC stoppage has **not** been reproduced
or declared fixed. New metrics/fault tests establish a way to distinguish stages.

## Verified SDK constraints and limits

The official current multi-device example confirms multi-device-aware connection
configuration, explicit device subscriptions and `device_id` routing. Official
function documentation says opening the connection is nonblocking; the first
poll establishes service communication. Event pointers last only until the next
poll/close, and concurrent polling is prohibited. The current converter copies
tracking values before the next poll, which should be preserved.

No LeapC development headers/libraries or physical devices are installed here.
Public documentation verification is not exact installed-SDK verification.
No proprietary SDK binary was downloaded, vendored or redistributed.

## Evidence and bounded implementation order

- [Tracking/runtime/UI audit and official API evidence](reports/TRACKING_AUDIT.md)
- [Build, dependency and test inventory](reports/BUILD_AUDIT.md)
- [Configuration evidence and regressions](reports/CONFIG_AUDIT.md)
- [OSC evidence and golden fixture scope](reports/OSC_AUDIT.md)

First freeze scalar OSC/config behavior with executable tests, repair the SDK-free
CMake path and config defects, then introduce a headless two-source vertical slice
with stale invalidation, isolated retries and stage counters. Preserve the real
adapter for Phase 2, where exact SDK compilation and physical evidence are required.
Qt/IPC, processing, bundling and versioned profiles remain later work.
