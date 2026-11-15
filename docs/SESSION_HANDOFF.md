# Handoff — native controller rebuild

Branch codex/handbridge-native-controller. Base30cd5e359ea410b1ad07ffc91e970f2d95bfb522.
Original master/handbridge-v2 and prior dev branch remain preserved.
Scope `91e59b8`; portable settings `2de385f`; independent engine IPC `99622f2`;
native GUI `9f5260ec60b017e1a4c6ba67845e6158fd987a2b`; CI logging
`5d1c4c2149e2fad092f9603b730e9f0334fa8653`. Final follow-up is documentation only.
Draft PR: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/2. Prior handoff archived
at reports/PHASE1_HANDOFF.md. The user explicitly authorized this modern native
GUI and real-settings slice ahead of physical adapter validation.

## Source changes

- src/runtime/ControlSettings, Processing, EngineRuntime: strict schema2/legacy
  import and real OSC filtering/routing/position transforms/units/gestures.
- src/engine/qt_engine_main.cpp, ConfigurationStore.hpp, src/ipc/Framing.hpp:
  independent worker, bounded same-user IPC/CAS/telemetry, monotonic atomic
  profiles/current+backups, startup restore and redacted diagnostic export.
- src/control/Controller.hpp/.cpp, control_main.cpp and12QMLfiles: VST-style native
  five-page controller, actual engine state, draft/ack/reject/save lifecycle,
  detached launch, literal serial paths, locale-safe editors, native screenshots.
- tests/test_ControlSettings.cpp, test_Processing.cpp, test_Controller.cpp,
  test_ControlUi.cpp, test_ControlIntegration.cpp, future-schema probe999 in
  existing test_HeadlessUdp.py. Default legacy packet fixtures unchanged.
- Optional Qt build targets/presets/workflow, new docs/configexample and actual
  native screenshot assets. No SDK/Qt binary/build cache is committed.

## Commands actually run

Local cloud tools installed under ignored work only; source work/qt-env.sh.

```sh
work/tooling/bin/cmake --preset control-debug   -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja"   -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64;$PWD/work/qt-deps/usr"
work/tooling/bin/cmake --build --preset control-debug --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software  XDG_CACHE_HOME="$PWD/work/qt-user-cache" XDG_DATA_HOME="$PWD/work/qt-user-data"  XDG_CONFIG_HOME="$PWD/work/qt-user-config" XDG_RUNTIME_DIR="$PWD/work/qt-runtime"  work/tooling/bin/ctest --preset control-debug --output-junit "$PWD/work/native-control-results.xml"
```

Final native58/58 PASS,zero skips/failures,18.42s. Plain Qt-free55/55 PASS3.76s.
ASan/UBSan56/56 PASS6.84s withleakchecking: project core/runtime/client/model/tests
instrumented; third-party Qt excluded, two GUI bundles excluded from sanitizer
run. Full unsanitized GUI/IPC bundles passed. Exact sanitizer recipe/pins in
reports/GUI_BUILD.md. ldd confirms engine QtCore/Network only.

Native all5page captures used actual processes +loopback UDP receiver and real
IPC-applied settings; no QML/render errors, every GUI and engine exited0.
work/previews contains scratch captures; docs/screenshots contains selected
reviewable native PNGs. Runtime continues after normalclose, kill and reopen;
QML port typing/direct Apply and transmit switch are verified against UDP.

## Fixed during review

Socket callbacks during Controller member destruction caused a heap failure;
explicit disconnect before member teardown fixed it, model/process tests pass.
Fixed stale/delayed ack handling, dirty notification, profile-load edit protection,
dotted serial paths, numeric parsing, direct Apply focus commit, >16-frame drain,
slow-client bounds, saved-snapshot ordering and initial-discovery Saved status.
No runtime image/SDK/hardware result is inferred from any of these tests.

## Verification boundary and blockers

Cloud native and mock/UDP behavior is verified. Native Qt Windows/MSVC and Ubuntu
CI passed at `5d1c4c2` as recorded below. Physical Windows SDK compile, USB/cameras/Hyperion,
TouchDesigner receipt and hardware soak remain UNTESTED ON HARDWARE.
New host still requires--simulate. No native hand IDs fabricated. No per-channel
processing overrides, full3D/all-joint view, OSCv2/bundles or release packaging.
Load can apply before persistence fails; response explicitly reports actual
applied-but-unsaved snapshot. GUI does not stop a detached engine; use foreground
Ctrl-C or OSprocessmanager for explicit shutdown.

## One next task

On the local Windows SDK PC, implement the smallest real LeapC ITrackingSource
adapter feeding this runtime/IPC/controller. Verify exact installed headers and
service versions; RAII handles/subscriptions, one bounded poll owner, owned frames
and stable serials. Keep mocks/goldens/native UI tests green, then record actual
two-camera, unplug/service recovery and TouchDesigner observations. Mark unavailable
physical steps BLOCKED. Do not rewrite the GUI again or add fusion/packaging.

## Final observed CI and residual uncertainty

At `5d1c4c2`, native run 43771500930:
https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43771500930

- Ubuntu: 58 passed, no failures/skips, 18.83 s.
- Windows Server 2022 / MSVC 19.44.35230: 57 passed, one expected POSIX-only skip,
  no failures, 35.03 s. All actual QML, GUI lifetime, profile/backup, settings and
  bounded client process checks passed. The integration bundle has nine behavior
  cases plus init/cleanup; none was disabled.

Qt-free run 43771500901: Ubuntu 55 passed; Windows 54 passed plus the same one
expected skip. Both jobs succeeded. Explicit native QtTest text logs, CTest log
and JUnit reports are retained by Actions.

Initial Windows run 43770816953 at GUI code `9f5260e` built but its broader process
bundle failed with empty stdout. No application or test behavior changed after
that failure; only file diagnostics were added. The rerun passed, so the original
cause remains unknown. Do not claim a product bug fixed or quietly suppress a
recurrence. Preserve CI logs and reproduce that case on the Windows PC if needed.

Final source ASan/UBSan rerun: 56 passed, 6.59 s, leak checking enabled, no errors.
The full unsanitized GUI suite passed separately; exclusions are documented above.
No real LeapC/USB/Hyperion/TouchDesigner test has run. Next task remains the local
installed-SDK adapter with actual two-camera observations, preserving this GUI.

## Follow-up on the initially unattributed Windows failure

A parallel push job (run43771496974, Windows132988183221) at5d1c4c2 failed while
the PR job passed. File logs identified an intermittent independent-client startup
failure and cascading alive-QProcess fixtures after failed init. Production GUI /
engine sources remain9f5260e. The test helper now connects asynchronously with
bounded retries, owns failed-start cleanup and includes stage diagnostics plus a
real late-server test. No output tolerance, test skip or total connect bound was
changed. Local58/58PASS19.87s; Windows until-fail three-run integration gate added.
Observe its result before describing the platform checks as stable.

## Preview truth and Windows stability checkpoint

At harness8367fad, Windows native full suite passed and the integration bundle
passed another three until-fail repetitions (four consecutive process-suite passes,
no retries-until-pass). Run43772576976 job132991553046: full57pass+1expectedskip;
repetitions23.74/20.84/20.51s allpass. Harness failure isolation now verified.

A final small QML correction makes live preview units/mode follow applied engine
config instead of unsent drafts. Raw bypass always labels/projects mm. Actual QML
regression covers before/after acknowledgement and bypass. Local58/58PASS19.98s.
Runtime/OSC sources unchanged; latest GUI commit/CI will be pinned in final notes.
