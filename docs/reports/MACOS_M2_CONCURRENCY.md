# M2 concurrency compatibility slice — 2026-11-16

Scope authorized after collecting M0 evidence, on
`codex/handbridge-macos-feasibility`. M0 SDK-only retry at e1fcd18 passed natively
on Intel Mac: [run 43962640624](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43962640624).
The client reports 6.2.0; connected ServerLibrary remains unavailable. Apple
Silicon offline loading is still blocked by hosted allocation, not an SDK failure.
Original Xcode 16.4 feature probes are not repeated; no downgrade or GUI redesign.

## Change and ownership

ADR-017 replaces the QtCore/Network host's jthread/stop-token and atomic shared
snapshot specialization. One std::thread still owns SDK/runtime/processor/OSC;
GUI/control I/O stay on the event thread. An atomic bool stop flag is set under
the command mutex before notifying its condition variable. Join happens outside
locks. Startup promise failures explicitly stop/join before constructor unwind;
the std::thread destructor must never terminate a failed startup. The single
control thread may stop repeatedly, and post-stop enqueue rejects new work.
The 32-job limit, two-ms wait and 100-ms telemetry cadence are unchanged.

SnapshotMailbox<T> holds one shared_ptr<const T> under its own short mutex.
Publication builds JSON first, swaps the pointer, then destroys the previous
value outside the lock. Readers copy ownership and can retain old immutable values
across new publications and mailbox destruction. No Qt/SDK I/O or serialization
occurs while the snapshot mutex is held.

## Actual verification

Local configure/build PASS with GCC 14.2.0 / Qt 6.8.3 / CMake 3.31.6 / Ninja
1.11.1 on Linux x86_64. Focused CTest **3/3 PASS**, 0.36 s, including both snapshot
cases and WorkerLifecycle. Full native Linux suite **64/64 PASS**, no skips/failures, **22.12 s**, including
actual GUI close/kill/reopen with continuing simulated UDP. Tested application
source **536a018b485e54b0eabbd250d01fcde72bc86545**. New tests use no real SDK or cameras.
[Mac host CI 43964527004](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43964527004),
[Windows/Linux native CI 43964526847](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43964526847),
[core CI 43964526874](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43964526874)
and [Windows Release CI 43964527518](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43964527518)
completed: core, native Windows/Ubuntu (including repeated Windows lifecycle)
and Windows Release/package CI **PASS**. The first Mac run failed the separate
broadcast SIGPIPE case; its Qt host build was skipped. All application source
remains identical through the independent Mac workflow revision **1579c4f**.
Initial local setup lacked CMake/Qt and OpenGL development headers; pinned tools
were installed under ignored work/, and Debian libglvnd 1.7.0-1+b2 development
packages extracted under work/qt-deps (no system install). Configure selected
installed /lib/x86_64-linux-gnu/libOpenGL.so.0 and libGLX.so.0 explicitly. No
application build/GUI workaround was introduced for this environment. New SDK-free checks cover concurrent
snapshot coherence and retained lifetime, worker startup failure without abort,
natural timed shutdown/join and final summary/UDP. Existing Windows/Linux suites
cover configuration acknowledgements, profiles, legacy OSC and continuing output
across GUI close/kill/reopen. Apple CI builds only the existing Qt-free host,
QtCore/Network host and test targets; it does not build a new GUI or package.

```sh
export PATH="$PWD/work/tooling/bin:$PATH"
export XDG_RUNTIME_DIR="$PWD/work/test-runtime"
export XDG_DATA_HOME="$PWD/work/test-data"
export XDG_CONFIG_HOME="$PWD/work/test-config"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_DATA_HOME" "$XDG_CONFIG_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
cmake --preset control-debug \
  -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64;$PWD/work/qt-deps/usr" \
  -DOPENGL_opengl_LIBRARY=/lib/x86_64-linux-gnu/libOpenGL.so.0 \
  -DOPENGL_glx_LIBRARY=/lib/x86_64-linux-gnu/libGLX.so.0 \
  -DOPENGL_INCLUDE_DIR="$PWD/work/qt-deps/usr/include"
cmake --build --preset control-debug --parallel 2
ctest --preset control-debug -R 'WorkerConcurrency|WorkerLifecycle' --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --preset control-debug
```

New `macos-intel-core` / `macos-intel-control` presets pin x86_64 and a **CI test
configuration** deployment target of 12.0 (Qt 6.8 minimum); this is not a released
application minimum or proof of execution on macOS 12. Workflow pins macos-15-intel,
Qt 6.8.3, CMake 3.31.6, Ninja 1.11.1.4 and dependency Actions. It records actual
Apple toolchain inputs and tests the replacement directly without rerunning the
known original-library probes. Mac GUI suites are excluded because no GUI target
is built. Windows/Linux GUI lifecycle regressions remain mandatory.

## First Apple run: a separate transport blocker

At application source 536a018, Mac CI 43964527004 compiled/linked the Qt-free host
and all core tests; **59/60 passed** (5.98 s). Both replacement SnapshotMailbox
tests and HeadlessUdp passed. The existing
`LegacyOscContract.NativeBroadcastPermissionErrorIsNotReportedAsSuccess` test
terminated with **SIGPIPE** on Darwin. This is a real Mac transport portability
finding, not a compiler/threading/SDK failure. The failed core step prevented the
QtCore/Network host step from executing; its result was **NOT RUN**, not a build
failure. No tests were removed and no sender behavior changed to hide this.

The workflow now runs core and Qt-host suites independently, preserves their
individual results and fails a final gate if either suite/build fails. This lets
startup/join and native host tests execute without reclassifying the known
SIGPIPE case as a pass. The independent rerun is recorded below. Full M2 must resolve that actual
Darwin error path separately before a passing Mac-suite claim or distribution.
Windows/Ubuntu native CI and Windows Release packaging passed at 536a018.
Log/artifact downloads intermittently returned HTTP 403; the failed Mac step log
was retrieved and established the exact failing test. No inaccessible remote
per-case totals are inferred from job success.

## Independent Apple host result — BUILD/MOCK TESTED

[Run 43965924196](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43965924196)
at **1579c4ffdd5569b698c826f40f60411894792698** completed. Both Qt-free and existing
QtCore/Network hosts **compiled and linked successfully in C++20**. The replacement
snapshot concurrency/lifetime tests passed in both suites; **WorkerLifecycle PASS**
(0.45 s) exercises the real native Qt host's startup failure (normal exit 1) and
natural timed stop/wake/join (exit 0, final summary and decoded simulated UDP).
No GUI executable was built on Mac. No SDK was fetched/used by these M2 tests.

Measured inputs: **x86_64**, macOS **15.7.9 / 24G830**, Xcode **16.4 / 16F6**,
Apple Clang **17.0.0 (clang-1700.0.13.5)**, target x86_64-apple-darwin24.6.0,
macOS SDK **15.5**, libc++ headers **190102**, Qt **6.8.3**. Build tools installed
successfully at pinned CMake **3.31.6** and Ninja package **1.11.1.4**. Presets use
x86_64 / deployment-target test configuration 12.0. The changed worker files
compiled; existing unused/deprecation warnings remain in unchanged core/oscpack.

| Executed suite | Result |
|---|---|
| Mac Qt-free | **59 PASS / 1 FAIL**, 60 total, 7.12 s |
| Mac QtCore/Network host and core (GUI tests excluded) | **60 PASS / 1 FAIL**, 61 total, 7.17 s |
| New snapshot tests | **PASS** in both builds |
| Qt host startup failure / natural shutdown / final snapshot / actual mock UDP | **PASS** |
| Failing case in each suite | NativeBroadcastPermissionErrorIsNotReportedAsSuccess: **SIGPIPE** |

**Overall Mac CI remains FAILURE**, deliberately preserved by the final gate.
Step conclusions may appear successful due to continue-on-error; retained CTest
output and the final gate are authoritative. No test was removed, relaxed, retried
until success or reclassified as a skip. HeadlessUdp and all other core/settings/
processing/legacy golden cases passed. This verifies the narrow concurrency slice;
it does **not** pass full M2. Logs and text-only test artifacts were uploaded;
full logs were retrieved and the numeric results above are observed, not inferred.
Windows/Linux per-case totals were not downloaded after transient HTTP 403s;
only their verified overall job/step success is reported alongside exact local
64/64 results.

## Next exact task

Diagnose the observed Darwin SIGPIPE in the existing broadcast-error test and
ensure a failed send is reported/counted rather than terminating the process,
without changing legacy OSC or Windows behavior; rerun both Mac suites. Keep this
a separate reviewable M2 task. Protected IPC, arm64 and physical M1 gates remain
open before production Mac integration/packaging.

## Limits

Narrow concurrency slice **implemented and BUILD/MOCK TESTED**. Full M2 remains
**in progress**: the Darwin SIGPIPE case, protected Mac endpoint mapping, ownership/symlink and
cross-user access checks, arm64 host matrix, full native Mac UI/lifecycle and
application-minimum decisions remain open. No Mac distribution is produced.
M1 physical streams/recovery and real tracking service/TouchDesigner receipt are
**NOT RUN**. Production Mac LeapC loader, packaging and hardware integration are
not changed. C++20, Hyperion 6.2 gates, Windows product, GUI, legacy OSC/config
and independent engine lifetime are preserved.
