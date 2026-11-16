# M2 concurrency compatibility slice — 2026-11-16

Scope authorized after collecting M0 evidence, on
`codex/handbridge-macos-feasibility`. M0 SDK-only retry at e1fcd18 passed natively
on Intel Mac: [run 43962640624](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43962640624).
The client reports 6.2.0; connected ServerLibrary remains unavailable. Apple
Silicon offline loading is still blocked by hosted allocation, not an SDK failure.
Original Xcode 16.4 feature probes are not repeated; no downgrade or GUI rebuild.

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

## Verification plan / actual status

Local configure/build PASS with GCC 14.2.0 / Qt 6.8.3 / CMake 3.31.6 / Ninja
1.11.1 on Linux x86_64. Focused CTest **3/3 PASS**, 0.36 s, including both snapshot
cases and WorkerLifecycle. Full suite and Mac/Windows CI pending at this checkpoint.
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
cmake --preset control-debug -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64"
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

## Limits

M2 only **in progress**: protected Mac endpoint mapping, ownership/symlink and
cross-user access checks, arm64 host matrix, full native Mac UI/lifecycle and
application-minimum decisions remain open. No Mac distribution is produced.
M1 physical streams/recovery and real tracking service/TouchDesigner receipt are
**NOT RUN**. Production Mac LeapC loader, packaging and hardware integration are
not changed. C++20, Hyperion 6.2 gates, Windows product, GUI, legacy OSC/config
and independent engine lifetime are preserved.
