# Current state — native VST-style controller

Branch: `codex/handbridge-native-controller`; base:
`30cd5e359ea410b1ad07ffc91e970f2d95bfb522` on preserved prior development branch.
Original master/handbridge-v2 and the legacy ImGui source remain unchanged.
Commits: approved scope `91e59b8`, portable settings `2de385f`, native IPC/profiles
`99622f2`, native GUI `9f5260ec60b017e1a4c6ba67845e6158fd987a2b`, diagnostic
logging `5d1c4c2149e2fad092f9603b730e9f0334fa8653`.
Draft [PR #2](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/2)
is stacked on the preserved prior foundation. Final follow-up changes docs only. Hardware is still
UNTESTED ON HARDWARE. User explicitly authorized this GUI/processing slice now.

## Implemented

- Separate Qt6 Quick/QML VST-style controller with five functional pages, editable
  dials, device routing, all14 channel flags, OSC target/rate/enable, profiles,
  raw/processed preview, measured diagnostics and clear edit/apply/save states.
- QtCore/Network-only headless IPC host; source/runtime/sender have an independent
  C++ worker. GUI/disk work cannot own tracking. Default Qt-free host preserved.
- Strict schema2 plus legacy import; actual filters/transforms/units, optional
  hysteresis channels, per-device mute/hand/alias routing and manual restart.
- Same-user bounded local protocol, CAS revisions, slow-client isolation, output
  reset, atomic profile/startup saves/backups and serial-redacted diagnostics.
- Regressions cover direct textfield Apply, dotted serials, decimal parsing,
  delayed ack/draft state, pending-load edits, monotonic saved snapshots and newly
  discovered aliases becoming unsaved.

## Actual verification

CLOUD Debian13/GCC14.2/Qt6.8.3/CMake3.31.6:

| Scope | Outcome |
|---|---|
| Default Qt-free Debug core/settings/process |55/55 PASS,0 skips,3.76s |
| Final native Qt build and complete CTest |58/58 PASS,0 failures/skips,18.42s |
| ASan+UBSan with leak checking |56/56 PASS,0 skips,6.84s; core/process +instrumented client/model, excludes two GUI bundles; third-party Qt not instrumented |
| Controller model bundle |9 scenarios +init/cleanup =11 passing checks |
| Actual native QML typing/clicks |New port→direct Apply→actual UDP destination; output toggle stops/resumes actual packets |
| Process integration bundle |9 behaviors +init/cleanup =11 checks,10.84s; includes actual GUI close/kill/reopen, persistence ordering/startup restore, malformed/slow clients and redaction |
| Native visual review |All5 pages captured at1360x860, each controller exit0, no QML warnings, engine exit0; processing knobs fit default window |
| Engine dependency boundary |ldd shows QtCore/Network only; no QtGui/Quick/Widgets |
| Native Ubuntu CI | 58 passed, zero failures/skips, 18.83 s |
| Native Windows CI / MSVC 19.44.35230 | 57 passed + 1 expected POSIX-only skip, zero failures, 35.03 s |
| Qt-free Ubuntu / Windows CI | 55 passed / 54 passed + 1 expected skip, zero failures |
| Physical Windows/SDK/USB/Hyperion/TouchDesigner |UNTESTED ON HARDWARE |

Commands/pins/sanitizer exclusions: [GUI_BUILD](reports/GUI_BUILD.md).
Actual native screenshots: [overview](screenshots/overview.png),
[processing](screenshots/processing.png), [OSC](screenshots/osc.png).
No image mockups or invented live metrics. Demo screenshots explicitly enable
processing (30ms/1.5mm/60Hz) through IPC; shipped defaults remain raw/no filters.

## Remaining limits

New source is still simulator-only; there is no real LeapC adapter or proven
field-stall fix. Current processing/channel controls are global, per-device
identity/mute/filter is supported. Native hand IDs, per-channel overrides, full
3D/all-joint preview, OSCv2/bundles, installer/deployment packages and physical
soak are not implemented. Load applies before startup persistence; a disk failure
returns explicit applied-but-unsaved state. The old plain legacy saver remains
non-atomic; atomic profiles belong to the new Qt host.

## Single next task

Build the real LeapC ITrackingSource adapter on the local Windows SDK machine,
using exact installed headers/service versions and this unchanged controller/IPC
boundary. Verify two cameras, USB/service recovery and actual TouchDesigner OSC,
recording real results or BLOCKED. Do not infer hardware success from these mocks.

## Observed CI and known transient result

At diagnostic commit `5d1c4c2`, [native Qt run 43771500930](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43771500930)
and [Qt-free run 43771500901](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43771500901)
completed successfully on Windows and Ubuntu. Windows actual QML typing/Apply/UDP,
GUI close/kill/reopen, all profile persistence and malformed/slow-client cases
passed. QtTest bundles retain explicit text logs in CI artifacts.

The first native Windows run at `9f5260e` built and passed the direct QML test but
failed the broader process bundle with empty stdout. The same application/test
source passed after diagnostic-only file logging was added. The initial cause
remains unknown and is not claimed fixed; keep the retained logs if it recurs.
No test was skipped or acceptance threshold widened to obtain that pass. The one
Windows skip is the previously documented POSIX kernel-permission test.

Final source sanitizer rerun: 56 passed, zero failures/skips, 6.59 s with leak
checking; no diagnostics. Exact instrumentation exclusions remain in GUI_BUILD.
This is still simulator/IPC/OSC evidence, not hardware or TouchDesigner validation.

## Windows startup test-harness correction

The parallel push run43771496974 at the same5d1c4c2 source did reproduce the
integration failure. Retained QtTest logs show the first three behavior cases
passed, then a later fixture failed to connect although the engine logged both
streams as live. Failed initialization left the member QProcess alive and later
setups cascaded with 'Process is already running'. This is test-harness evidence;
no production GUI/engine defect is inferred or silently patched.

The independent test client now uses bounded asynchronous connection attempts
instead of repeated20ms blocking waits. Every failed startup owns cleanup, and
case setup clears any prior process. Failure messages record process/socket/request
stages. A real delayed-server regression was added. Original total5s connect
bound and output assertions are unchanged. Local58/58PASS,19.87s. Windows CI now
runs the integration bundle three times using until-fail, never until-pass.
Final Windows stability results will be recorded before completion.

## Live preview metadata correction

The Overview preview now reads acknowledged engine processing settings rather
than the editor draft. It remains in mm during raw bypass even if inactive
processed-unit settings prefer meters. A native QML regression verifies units
stay unchanged before Apply, change after acknowledgement, and return to raw mm
when bypass is applied. No OSC/runtime processing behavior changed. Full local
58/58 PASS, 19.98 s, zero failures/skips; final native CI follows this small UI fix.
