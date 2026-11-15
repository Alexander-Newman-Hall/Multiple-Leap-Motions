# Current state — native VST-style controller

Branch: `codex/handbridge-native-controller`; base:
`30cd5e359ea410b1ad07ffc91e970f2d95bfb522` on preserved prior development branch.
Original master/handbridge-v2 and the legacy ImGui source remain unchanged.
New commits: approved scope `91e59b8`, portable settings `2de385f`, native IPC /
profiles `99622f2`; GUI commit follows this tested checkpoint. Hardware is still
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
| New native Windows/Linux CI |Configured, execution pending publication |
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
