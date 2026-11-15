# Current state — native Hand Bridge controller

Branch: `codex/handbridge-native-controller`.
Draft [PR #2](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/2)
is stacked on the preserved Phase 0/1 branch at `30cd5e3`.
Original `master`, `handbridge-v2` and ImGui source remain unchanged.

Latest application/UI commit: **`ebb3bd92ce04aa011cfdbaacbdce0adfc67beb1f`**.
Milestones: scope `91e59b8`, settings `2de385f`, native IPC `99622f2`, controller
`9f5260e`, explicit test diagnostics `5d1c4c2`, Windows harness isolation `8367fad`.
Final documentation-only follow-up records CI without changing application code.

## Implemented

- Native Qt 6 Quick/QML controller with a VST-style dark interface, editable
  rotary controls and five functional pages: Overview, Devices, Processing,
  OSC Output, Profiles & Diagnostics.
- All supported schema settings: fourteen channel flags; per-device alias,
  mute/hand routing/restart; output destination/enable/rate/reset; positional
  smoothing/deadzone, scale/rotation/offset/units and optional gesture hysteresis.
- A separate Qt Core/Network engine with an independent source/processing/UDP
  worker. GUI closure, crashes and profile file I/O do not own tracking lifetime.
- Bounded same-user native IPC, engine validation/CAS acknowledgements,
  Editing/Applied/Rejected/Saved states, atomic profiles/startup backups,
  measured telemetry and serial-redacted diagnostic export.
- Live preview metadata follows acknowledged engine settings, not pending
  edits; raw bypass always projects and labels millimeters.

Default Qt-free console build remains available. Raw bypass, localhost:9000,
legacy scalar messages, case and packet goldens are preserved. The new tracking
source is still **simulation-only**; no real LeapC adapter is present.

## Verified results

| Environment / scope | Result |
|---|---|
| Cloud GCC 14.2 / Qt 6.8.3 final native suite | 58 passed, zero failures/skips, 19.98 s |
| Cloud Qt-free core/settings/process | 55 passed, zero skips |
| Cloud ASan/UBSan with leak checking | 56 passed, zero diagnostics; core/process + client/model; excludes two GUI bundles and third-party instrumentation |
| Native Windows/Ubuntu source CI | Final application compile/full suite passed; final repeated Windows gate evidence follows below |
| Actual QML interaction | Typed port → direct Apply → actual UDP destination; transmit switch stops/resumes packets; preview units change only after apply |
| Separate-process integration | GUI close/kill/reopen preserves engine PID and changed UDP; settings, profile ordering/restart, malformed/slow clients and delayed server verified |
| Native visual review | All five pages captured at 1360×860; each controller and engine exited 0 without QML errors |
| Headless dependency boundary | Qt Core/Network only; no Qt Gui/Quick/Widgets |
| Physical Windows SDK/cameras/USB/Hyperion/TouchDesigner | **UNTESTED ON HARDWARE** |

Commands, package/action pins, sanitizer exclusions and full CI logs:
[GUI_BUILD](reports/GUI_BUILD.md). Native screenshots:
[Overview](screenshots/overview.png), [Processing](screenshots/processing.png),
[OSC](screenshots/osc.png). Demo screenshots explicitly enable processing;
shipped defaults remain raw/no filters.

The original Windows process-test intermittency was diagnosed through retained
QtTest logs: an independent test client startup failure left a fixture process
alive and caused cascading setups. The harness now uses bounded asynchronous
connection and failed-start cleanup, with unchanged five-second total connection
bound and unchanged output assertions. Windows integration has passed repeated
`until-fail` runs; no until-pass retry or skip conceals this case.

## Remaining scope

No proven real-camera field-stall fix, native hand IDs, per-channel processing
overrides, full 3D/all-joint preview, OSCv2/bundles, installer or hardware soak.
Current processing/channel controls are global; routing/identity is per-device.
A profile load can apply before persistence fails; its response explicitly
reports applied-but-unsaved state. Legacy direct save remains non-atomic; atomic
profiles belong to the new Qt host.

## Single next task

On the local Windows SDK PC, add the smallest real LeapC ITrackingSource adapter
using exact installed headers/service versions. Preserve this controller/IPC and
tests; verify two devices, unplug/service recovery and actual TouchDesigner OSC.
Record real observations or BLOCKED. No hardware success follows from these mocks.


## Final native source CI — ebb3bd9

Tested application/UI: `ebb3bd92ce04aa011cfdbaacbdce0adfc67beb1f`.
[PR native run 43773045258](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43773045258):
Ubuntu 58 passed, no skips/failures (20.32 s). Windows Server 2022 / MSVC
19.44.35230: 57 passed + one expected POSIX-only skip, zero failures (32.43 s).
The Windows integration bundle passed three additional until-fail repetitions:
19.97 / 19.95 / 20.02 s, all twelve Qt checks passing each time.

The independent [push run 43773039247](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43773039247)
also passed on both platforms: Ubuntu 58 passed; Windows 57 passed + the same
expected skip, then three more passing integration repetitions. This verifies
the final applied-preview metadata regression as well as the corrected harness.
No source/runtime assertions were relaxed, no case was retried until success,
and no hardware result is inferred. The final handoff commit changes documentation only; application and test
source are identical to the tested commit above.
