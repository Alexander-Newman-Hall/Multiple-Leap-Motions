# Native controller build and verification

Branch: `codex/handbridge-native-controller`, based on `30cd5e3`.
This report covers C++20 / Qt6 Quick/QML GUI and local IPC builds. No LeapC SDK
binary is downloaded or required for the simulated path. Hardware remains
UNTESTED ON HARDWARE. Final source SHA/results are in CURRENT_STATE/SESSION_HANDOFF.

## Target boundaries

Default `HANDBRIDGE_BUILD_CONTROL=OFF` preserves the Qt-free headless host/core
build. Pure validated settings/processing sources and their tests remain SDK/Qt
independent. With `HANDBRIDGE_BUILD_CONTROL=ON`:

- `handbridge-engine`: new console IPC host + existing runtime; links Qt Core
  and Network only. No QtGui/Quick/Widgets dependency belongs to this process.
- `handbridge-control-client`: Qt Core/Network C++ authoritative-state client model.
- `handbridge-control`: separate Qt Quick/QML executable, embedded local resources
  at `qrc:/handbridge/Main.qml`; no browser, web server or web dependency.
- `test_Controller`: ack/dirty/reconnect model coverage against a controlled server.
- `test_ControlIntegration`: external-process engine/controller lifecycle + UDP
  and settings integration. QtTest is a development/test dependency only.

Old `leapMotionApp` source remains opt-in legacy reference; no destructive source
removal or original-branch rewriting. New engine still requires simulated source;
actual SDK adapter is pending. QML is dynamically loaded from embedded resources
and exercised by actual controller runs; visual inspection is separate from core tests.

## Developer commands

Install Qt6.8.3 official desktop development files under its license (GCC Linux or
MSVC2022 x64 Windows). Supply its prefix using CMAKE_PREFIX_PATH or Qt6_DIR.
Linux needs desktop OpenGL/EGL and xkbcommon development headers/libraries. Python
is test infrastructure only; no Python code runs in the engine/controller.

```sh
cmake --preset control-debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
cmake --build --preset control-debug --parallel 2
ctest --preset control-debug
./out/build/control-debug/handbridge-control --demo
```

```powershell
cmake --preset windows-control -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset windows-control --parallel 2
ctest --preset windows-control
.\out\build\windows-control\Debug\handbridge-control.exe --demo
```

`--demo` launches a detached simulated engine; closing/crashing the controller
must leave it alive. For explicitly manual engine launch use its `--help`,
`--simulate`, endpoint/config arguments documented in CONTROL_PROTOCOL.

## Cloud-only development setup

The managed cloud environment has no Qt dev files, development GL/XKB headers,
system package helper or sudo. Installed only under ignored `work/`:

- CMake3.31.6/Ninja wheel1.11.1.4 from existing work/tooling venv.
- aqtinstall3.3.0 via that venv; official Qt6.8.3 `linux_gcc_64` archives qtbase,
  qtdeclarative, qttools and ICU. These are development dependencies, not vendored
  in git. No proprietary Ultraleap SDK is fetched.
- Read-only system had runtime GL/EGL/XKB libraries but no headers. Extracted
  official Debian libgl-dev/libglx-dev/libegl-dev/libopengl-dev1.7.0-1+b2 and
  libxkbcommon-dev1.7.0-2 under work/qt-deps/usr; linked their versioned symlinks
  to matching existing system runtime libraries. No system files changed.
- aqt3.3's import initialized a home-directory temporary folder before processing
  CLI config; its local venv settings.ini was pointed to ignored work/aqt-data.
  No HOME/CODEX_HOME changes, TLS disabling, repository or sudo policy changes.

Working cloud prefix:

```sh
. work/qt-env.sh
cmake --preset control-debug \
  -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja" \
  -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64;$PWD/work/qt-deps/usr"
cmake --build --preset control-debug --parallel 2
ctest --preset control-debug --output-junit "$PWD/work/native-control-results.xml"
```

Qt package probe confirmed Core/Network/Quick/Qml/QuickControls2/Test6.8.3 resolves
with GCC14.2. Vulkan headers are absent; CMake reports that optional feature but
configuration succeeds. Software/offscreen QtQuick is used for cloud UI tests.
Final actual build/test results follow after integration; no PASS is inferred
from the dependency probe alone.

## CI and distribution boundary

`.github/workflows/native-control.yml` adds Ubuntu24.04 and Windows2022 builds,
Qt6.8.3, pinned immutable install-qt-action v4.3.0 revision and pinned helper
versions. Core-mock workflow is unchanged. GUI CI uploads test evidence only;
it does not package or label a simulated binary as a hardware product. Review
Qt/OpenGL/third-party license notices before any binary redistribution.

## Actual cloud integration results

Real GCC14.2/CMake3.31.6 configure and native link succeeded with Qt6.8.3.
`ldd out/build/control-debug/handbridge-engine` reports Qt6Network and Qt6Core;
there are no QtGui/Quick/Widgets dependencies in that headless executable.
The standalone host smoke run emitted simulated metrics and real native UDP
errors on an unopened destination, rather than claiming receiver acknowledgement.

After final QML ranges/dirty-state fixes and client lifetime correction:

```sh
cmake --build --preset control-debug --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  XDG_CACHE_HOME="$PWD/work/qt-user-cache" \
  XDG_DATA_HOME="$PWD/work/qt-user-data" \
  XDG_CONFIG_HOME="$PWD/work/qt-user-config" \
  XDG_RUNTIME_DIR="$PWD/work/qt-runtime" \
  ctest --preset control-debug --output-junit "$PWD/work/native-control-results.xml"
```

**PASS58/58 CTest entries, zero failures/skips,18.17s.** This includes54 discovered
native core/settings/processing tests, the existing process/UDP test and three
QtTest bundles. The model bundle reports11 checks passing (9 scenarios plus
init/cleanup); actual QML clicks drive UDP off/on; external-process integration
verifies routing, processing, output rate/destination, atomic profile persistence,
malformed/slow clients, duplicate engine rejection, actual GUI close/kill/reopen
without stopping UDP, native PNG capture and redacted diagnostics export.

Initial integration found real fixes: GUI-model socket callbacks during destructor
caused heap corruption (fixed by explicit disconnect before member destruction);
QML object-tree tests needed visual-item traversal; schema2 is now supported by
the Qt host so the old future-version rejection probe moved to999; screenshot
checks now await complete PNG output rather than file-existence alone. All pass
in the final run. Native images are rendered software/offscreen in cloud; physical
Windows/Ultraleap/TouchDesigner and deployment remains UNTESTED ON HARDWARE.
CI workflow execution will be recorded in current state after publication.

## Sanitizer follow-up after peer review

`HANDBRIDGE_ENABLE_SANITIZERS=ON` now instruments optional controller/client/QtTest
C++ targets too, not only the runtime propagated through handbridge-core. Built
an isolated `out/build/control-sanitized` with the same Qt/dependency prefixes,
`-DHANDBRIDGE_BUILD_CONTROL=ON -DHANDBRIDGE_ENABLE_SANITIZERS=ON` and Debug. Built
these targets with `--parallel 2`: `test_Controller handbridge-engine
 test_ConfigManager test_DeviceAliasManager test_LeapSorter test_LegacyOscContract
 test_EngineRuntime test_ControlSettings test_Processing`.

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir out/build/control-sanitized --output-on-failure \
  -E 'ControlUi|ControlIntegration' \
  --output-junit "$PWD/work/native-control-sanitized-results.xml"
```

**PASS56/56 CTest entries,0 failures/skips,6.84s**, with no sanitizer diagnostics.
This covers55 core/settings/processing/process tests plus ControllerModel (9
scenarios/11 QtTest checks), including literal-serial editing and pending-profile
load guards. Client/model code and test executables are instrumented; third-party
Qt/oscpack/GoogleTest libraries are not instrumented. The actual GUI rendering and
external-process GUI integration bundles are excluded from this sanitizer run;
they are exercised in the normal58-entry suite. No physical-hardware claim follows.

## Frozen-source integration checkpoint

After final peer fixes (C-locale numeric editors, focused editor/direct Apply,
literal serial keys, pending profile-load guards and monotonic current-file
persistence), the source/resource rebuild and complete normal suite passed again:
**58/58 CTest entries,0 failures/skips,18.59s**. ControllerModel passed in1.93s;
ControlUi in1.83s now types a new port and clicks Apply directly, verifying real
UDP moves to the new receiver; ControlIntegration passed in10.99s with9 behavior
cases/11 QtTest checks, including stale revision write rejection and restored
minimal config discovery producing an unsaved newer revision. These supersede
initial18.17s timing only; the earlier58-case result remains a checkpoint.
No remaining build/test processes are owned by the build agent.


## Root final rendered-source verification

After the last visual-only processing layout polish, root rebuilt embedded QML
resources and reran the complete suite:58/58 PASS,0fail/skip,18.42s. Actual QML
port typing/directApply and UDP switch checks passed; external-process bundle
9behaviors/11Qtchecks passed10.84s. All5native pages were captured1360x860 with
no stderr/QML warnings and exit0, followed by clean engine exit0. Selected PNGs
in docs/screenshots are native output, not a design mockup. This supersedes the
previous18.59/18.17s checkpoints; sanitizer scope remains separately stated.

## First observed native CI checkpoint

At source `9f5260e`, native Qt workflow run43770816953 configured and built on both
Ubuntu24.04/GCC13.3 and Windows2022/MSVC19.44.35230. Ubuntu passed58/58 (19.02s).
Windows passed core/process/model/actual QML interaction groups, skipped the one
explicit POSIX kernel-denial case, and failed only ControlIntegration (50.15s);
its default QtTest console output was empty in both Actions and LastTest.log.
The Windows failure is not yet attributed to product or harness behavior.

CTest now requests one explicit text-file logger per QtTest bundle (`qt-*.txt`),
and native CI prints/uploads those diagnostics alongside JUnit and LastTest.log.
Local ControllerModel confirmed that logger works and still passes11 checks.
This is diagnostics only: no test was removed/skipped or timeout inflated, and no
production change is inferred from missing output. Final rerun evidence belongs
in CURRENT_STATE/SESSION_HANDOFF; physical hardware remains untested.

## Observed native Windows/Linux CI pass

Diagnostic-only commit `5d1c4c2149e2fad092f9603b730e9f0334fa8653`, native workflow
[run43771500930](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43771500930):

- Ubuntu24.04/GCC13.3: configure/build PASS,58/58 CTest entries PASS,0 skips,
  18.83s. Model11 checks, native QML interaction3 checks, integration11 checks.
- Windows Server2022/MSVC19.44.35230: configure/build PASS,57 CTest entries PASS
  +1 explicit POSIX-only skip,0 failures,35.03s. Model11 checks PASS; actual native
  QML typing/Apply/UDP3 checks PASS; all9 integration behaviors/11 QtTest checks
  PASS, including controller close/kill/reopen, persistence and unread clients.
- Qt text-file diagnostic logs were printed and included in uploaded artifacts.

Production source is unchanged from9f5260e. The initial Windows Integration
failure with missing console output was not reproduced; explicit logging made
the successful checks inspectable. Its original cause remains unknown, so no
production bug fix or hardware reliability claim is inferred from the rerun.
Windows offscreen Qt reported a font-directory fallback warning; no Windows
manual visual review or physical camera test was performed. Linux native images
were separately rendered and inspected by root. Hardware/SDK/TouchDesigner
acceptance remains pending on a local Windows PC.


Final source sanitizer recheck by root: 56/56 passed in 6.59 s, zero failures/skips,
ASan/UBSan and leak checking enabled. The latest normal native render/resource
suite passed 58/58 in 18.42 s. Application source is `9f5260e`; green CI at
`5d1c4c2` adds only explicit QtTest diagnostic file logging, not application fixes.

## Same-source push-run discrepancy — investigated before completion

The separate push-event run43771496974 at the same5d1c4c2 also had a Windows
Integration failure, now visible in its file logger (job132988183221). First3
integration behaviors passed; the next fixture init returned false from startEngine
while engine stderr showed both cameras STREAMING. Later init attempts reused an
already-running QProcess and failed in a cascade. Thus the single passing PR run
above is insufficient evidence of a stable harness. The exact first startup
failure remains a harness/IPC timing question; no camera/product fix is claimed.

The integration-test owner is repairing asynchronous startup and guaranteed
failure cleanup/diagnostics. The Windows workflow will run an additional
`--repeat until-fail:3` Integration check after the full suite: any failed attempt
fails the step, unlike retry-until-pass. Final outcome must be recorded after that
change, and no test is skipped or timeout inflated to hide the discrepancy.

## Windows integration harness correction and repeat gate

The test client's connection bootstrap now pumps asynchronous QLocalSocket state
and retries incomplete attempts cleanly instead of interleaving20ms blocking
waitForConnected calls with Windows named pipes. The5s overall deadline is
unchanged. Failed starts stop their child process and retain stage/process/socket
and request diagnostics, preventing an initial failure from cascading into later
fixtures. A delayed real local server regression exercises retry after the first
connection attempt. This changes test infrastructure, not engine/controller code.

Local complete suite after the correction: **58/58 PASS,0 failures/skips,19.87s**;
Integration now has10 behavior cases/12 QtTest checks, PASS12.13s. Windows native
CI adds an explicit post-suite `ctest -R '^ControlIntegration$' --repeat
until-fail:3`; any failure stops/fails the step. Repeat JUnit output is uploaded.
The next Windows run is required before claiming the mixed prior CI is resolved.
