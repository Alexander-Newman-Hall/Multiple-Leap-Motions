# Initial static repository audit — issues for Codex to verify

**Repository:** https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions
**Reference branch/commit observed 2026-11-15:** `master` at `054e578525a5d5202ea8f6eb417ce55ded7d61b8` (GitHub metadata). **Method:** static inspection of repository source and README. **NOT built or tested** as part of this planning kit.

## Source layout inventory

| Existing source | Role / concern |
|---|---|
| `src/main.cpp` | WinMain, Windows system-tray logic, SDL2/ImGui UI initialization and AppCore ownership. |
| `src/app/AppCore.cpp` | Orchestrates frame queue, LeapInput, LeapSorter, DataProcessor, OscSender, UI callbacks. |
| `src/pipeline/00_LeapConnection.cpp` | LeapC connection creation. Sets `eLeapConnectionConfig_MultiDeviceAware`. |
| `src/pipeline/01_LeapPoller.cpp` | Device discovery/open, `LeapSubscribeEvents`, event polling, conversion from `LEAP_TRACKING_EVENT`. |
| `src/core/LeapInput.cpp` | Poll worker, latest frame and `SpscQueue<FrameData>` push. |
| `src/pipeline/02_LeapSorter.cpp` | Device-specific handedness filtering. |
| `src/pipeline/03_DataProcessor.cpp` | Produces individual OSC scalar messages; raw mm positions, pinch/grab and optional fields. |
| `src/pipeline/04_OscSender.cpp` | OSC UDP transport; `sendBundle` currently unimplemented stub. |
| `src/core/ConfigManager.cpp` | Loads/saves settings, current default fallback port 9000, app data location. |
| `src/core/DeviceAliasManager.cpp` | Persistent `devN` serial→alias mapping, suspected JSON schema inconsistency. |
| `src/ui/MainAppWindow.cpp`, `UIController.cpp` | Legacy ImGui/SDL2 display and settings callbacks; UI state requires integration audit. |
| `CMakeLists.txt`, `leapMotionApp.sln`, `leapMotionApp.vcxproj` | Separate build definitions; audit source target parity, absolute paths and dependency versions. |
| `tests/` | Existing tests; some may be stale vs current interfaces; build them before refactoring. |
| `build/` | Tracked generated build artifacts: should be removed from future version control only after clean restore/build has been established. |

## Source-level findings (not confirmed field root causes)

### A. Alias serialization mismatch

`src/core/DeviceAliasManager.cpp::toJson()` places aliases under `deviceAliases`. Meanwhile, `src/core/ConfigManager.cpp::saveConfigToFile()` attempts to retrieve `deviceAliasManager.toJson()["device_aliases"]`, and load passes an object to `loadFromJson()` that looks for `deviceAliases` as an inner key. This mismatch is a likely configuration round-trip defect. Codex should reproduce with a unit test before making a fix. Unhandled `[]` behavior depends on JSON API and object constness; do not assume successful persistence.

### B. No complete service-level reconnection path

`src/pipeline/01_LeapPoller.cpp` handles `eLeapEventType_ConnectionLost` by notifying `LeapInput`; the inspected path does not demonstrate full recreation of connection, device handles, serial enumeration and subscriptions. Reconnection should be proven by lifecycle tests and a Windows service-restart test. The original reported 'UI runs but output stops' MAY be caused by service loss, but this is not established.

### C. Silent or poorly visible queue drops

`src/core/LeapInput.cpp` uses `SpscQueue<FrameData>`; `try_push` failure is only reported when verbose Leap logging is enabled. The dropped-frame counter should be unconditional and visible. Confirm number of actual poll threads/producers before retaining the SPSC model.

### D. High-frequency logging in the OSC path

`src/app/AppCore.cpp` logs per OSC data message in its `DataProcessor` callback; `src/pipeline/04_OscSender.cpp` loops over individual messages with console output. This may cause overhead under two-camera use and worsen queue backlog. Measure costs with debug logging off and high-rate synthetic streams.

### E. Bundle transport is not implemented

`src/pipeline/04_OscSender.cpp::sendBundle` is explicitly a stub. Add an efficient bounded OSC bundle implementation behind an **optional** compatibility-aware switch after golden tests; keep working scalar sends until tested in TouchDesigner.

### F. Data identity / validity gaps

`src/core/FrameData.hpp` stores `deviceId`, `timestamp`, hands, but no source frame sequence. `src/core/HandData.hpp` stores handedness, skeleton/confidence but not clearly the native `LEAP_HAND` ID. Preserve IDs and validity so smoothing state and future processing can reset correctly on re-acquisition.

### G. Duplicate/ambiguous handedness paths

`src/pipeline/02_LeapSorter.cpp` filters by hand assignments, while `src/pipeline/03_DataProcessor.cpp` independently queries an alias manager's hand-assignment mode. `src/core/DeviceAliasManager.cpp::getAssignedHand` includes TODO behavior. Audit which path actually takes precedence and consolidate one authoritative filter behavior with tests.

### H. Legacy configuration and outputs need measured baseline

`src/core/ConfigManager.cpp` uses `osc_port` default 9000. Some human-facing instructions have shown 7000. `src/pipeline/03_DataProcessor.cpp` produces lowercase hand type `left`/`right` from LeapPoller, addresses `/leap/{alias}/{hand}/...`, one float per message, and raw positions in mm. Preserve existing actual deployed behavior, including cases where original sends zero on missing hands; capture fixtures first.

### I. GUI runtime coupling and possible disconnected controls

`src/main.cpp` constructs UI callbacks including placeholders and `AppCore` with `MainAppWindow&`. `AppCore.cpp` sends frames to UI. The runtime/GUI separation is therefore incomplete. Determine exactly which UI controls are wired, not just drawn, and why visual state may appear stale. The new engine must not depend on UI lifetime or event loop.

### J. Build/test inconsistency

`CMakeLists.txt` targets C++17 in the original snapshot. The repository also has a Visual Studio `.vcxproj` and checked-in `build/` directory. Reconcile source/dependency targets, remove hard-coded local paths where present, and ensure tests compile against actual current interfaces. Do not delete generated tracked files until migration branch has clean reproducible builds.

## Field diagnosis priority

1. Make a deterministic mock/replay build and add stage-boundary counters.
2. Reproduce a stall by injecting poll errors, service loss, device loss, queue fill, processing failure and UDP failures separately.
3. Observe which metric stops first when real cameras fail; avoid premature 'fixed' claims.
4. Add recovery and verify that it restores stream from a known failure without affecting the other device.
5. Compare real OSC packets with golden fixture and TouchDesigner receiver observations.

## Evidence standard

Each claim in this document is a static code observation or test hypothesis. Codex must reread exact files at pinned commit, update line numbers and affected paths, run appropriate tests, and record whether the hypothesis is confirmed, rejected, or still unknown.
