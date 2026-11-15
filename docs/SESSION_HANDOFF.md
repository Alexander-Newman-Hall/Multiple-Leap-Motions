# Session handoff — native VST-style controller

Branch: `codex/handbridge-native-controller`.
Base: `30cd5e359ea410b1ad07ffc91e970f2d95bfb522` (preserved prior foundation).
Draft PR: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/2
Latest application/UI: **`ebb3bd92ce04aa011cfdbaacbdce0adfc67beb1f`**.
Earlier source checkpoints: portable settings `2de385f`, IPC/profile host `99622f2`,
GUI `9f5260e`, diagnostics `5d1c4c2`, Windows test harness `8367fad`.
The final follow-up changes only documentation. Prior Phase 1 handoff is archived
at reports/PHASE1_HANDOFF.md. Original branches/source are untouched.

The user explicitly authorized a complete modern native GUI with real settings
in this task, ahead of physical SDK work. GUI/processing scope is implemented;
hardware adapter acceptance is still blocked on the local PC.

## Changed modules

- `src/runtime/ControlSettings.*`, `Processing.*`, `EngineRuntime.*`: strict
  schema 2 plus legacy import; real OSC route/channel/mute/rate controls,
  time-based filters/transforms/units, opt-in pinch/grab hysteresis, previews.
- `src/engine/qt_engine_main.cpp`, `ConfigurationStore.hpp`, `src/ipc/Framing.hpp`:
  independent worker, bounded same-user protocol/CAS/telemetry, atomic named /
  startup profiles and backup ordering, redacted diagnostics.
- `src/control/Controller.*`, `control_main.cpp`, twelve QML files: five native
  pages, dials, acknowledged drafts/save state, detached engine launch, correct
  live preview metadata, literal serial paths and locale-safe editors.
- New settings/processing/model/QML/process tests; existing future-schema probe
  changed to 999 because the Qt host now supports implemented schema 2.
- Optional Qt CMake presets/targets and pinned native Windows/Linux workflow,
  studio-default JSON, protocol/schema/build guides and actual native screenshots.

## Exact cloud commands

Dependencies/tools live only under ignored work. Source `work/qt-env.sh` first.

```sh
cmake --preset control-debug   -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja"   -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64;$PWD/work/qt-deps/usr"
cmake --build --preset control-debug --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software  XDG_CACHE_HOME="$PWD/work/qt-user-cache" XDG_DATA_HOME="$PWD/work/qt-user-data"  XDG_CONFIG_HOME="$PWD/work/qt-user-config" XDG_RUNTIME_DIR="$PWD/work/qt-runtime"  ctest --preset control-debug --output-junit "$PWD/work/native-control-results.xml"
```

Final cloud: **58 passed, no failures/skips, 19.98 s**. Plain core: 55 passed.
ASan/UBSan: 56 passed in 6.59 s with leak checking; project core/process/client /
model instrumented, third-party libraries and two GUI bundles excluded. Full
unsanitized GUI bundles passed separately. Exact sanitizer commands in GUI_BUILD.
`ldd` confirms the host links Qt Core/Network only, never rendering libraries.

All five native pages were rendered and inspected at 1360×860 with no QML errors;
GUI and engine exited 0. Selected PNGs are in docs/screenshots. These are real
native captures with a mock source and actual loopback UDP, not image mockups.

## Acceptance evidence

Actual QML tests type a new OSC port then directly click Apply and observe the
new UDP receiver; output enable stops/resumes packets. Preview units remain at
engine values before ack and obey raw bypass after ack. Model tests cover delayed
acknowledgements/conflicts, dirty notifications, nested/dotted serial editing,
profile-load edit protection and reconnect state.

Process tests cover GUI normal close/kill/reopen while engine/output survive,
actual processing/alias/channel/rate effects, atomic profiles/restart/backups,
stale persistence completions, startup discovery becoming unsaved, delayed local
server, malformed/burst/unread clients, duplicate engine and redacted export.
Integration now has ten behavior cases plus init/cleanup (twelve Qt checks).
Windows adds an until-fail three-run gate; it fails on the first bad repetition.

Retained Windows logs identified a test-helper startup/cleanup cascade. The
independent client now uses bounded asynchronous connection and every failed
startup owns cleanup. No production tracking fix is inferred, no output assertion
was relaxed, and the original total connect bound stays five seconds. Final CI
results are recorded below and in GUI_BUILD, including this history.

## Limits and next task

Simulation-only. Physical SDK/USB/Hyperion/two cameras/TouchDesigner and soak are
UNTESTED ON HARDWARE. No fabricated native hand IDs, per-channel processing
inheritance, full 3D/all-joint view, OSCv2/bundles or installer. Profiles can be
applied before a persistence error; authoritative applied-but-unsaved state is
returned. Closing the GUI deliberately leaves the detached engine alive; use
foreground Ctrl-C or the OS process manager for explicit shutdown.

**Single next task:** on the local Windows SDK machine, implement the real LeapC
ITrackingSource adapter with exact headers/service versions, RAII, explicit
subscriptions, one bounded poll owner and owned serial-tagged frames. Keep this
controller and all mocks/goldens/UI tests, then record two-camera/recovery and
actual TouchDesigner observations or BLOCKED. Do not redesign the GUI or add
fusion/packaging in that task. Setup: WINDOWS_LOCAL_HANDOFF.md.


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
