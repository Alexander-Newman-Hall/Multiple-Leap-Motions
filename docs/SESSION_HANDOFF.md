# Session handoff — arm64-only SIGPIPE and native validation

Branch: `codex/handbridge-macos-feasibility`; starting source 48f2e17.
**Mac product scope is native Apple Silicon arm64 only.** No Intel build, Rosetta
or universal release requirement. Historical Intel results do not pass arm64
acceptance. Windows x64, C++20, Hyperion 6.2, Qt GUI and legacy OSC/config remain.

Darwin sender now sets SO_NOSIGPIPE per socket before connect/send, checking errors
through existing RAII/counters. Global signal policy is untouched. Existing
broadcast regression remains; new Darwin child test defaults/unblocks SIGPIPE and
checks error counting plus unchanged disposition. ADR-018 records the decision.

Changed paths: OscSender, legacy OSC tests, arm64-only presets/Mac workflows,
SDK fetch/diagnostic architecture guards, native GUI smoke, plan/spec/README,
OSC_PROTOCOL/DECISIONS and this state/handoff. [Arm64 report](reports/MACOS_ARM64.md)
separates new results from [historical M0](reports/MACOS_M0.md) and
[historical concurrency evidence](reports/MACOS_M2_CONCURRENCY.md).

| Acceptance | Current result |
|---|---|
| Linux local regression | Build, legacy OSC **8/8 PASS** (0.05 s), full native suite **64/64 PASS** (22.49 s); not Darwin/arm64 evidence |
| Native arm64 engine / unchanged GUI / genuine 6.2 SDK | CI prepared; actual arm64 results pending; no Intel substitution |
| Local Apple Silicon access | Not available; attached machine is Linux x86_64 |
| Physical Mac service / two cameras / recovery / TD | **NOT RUN** |

The arm64 job builds/tests engine and GUI, checks application slices, loads the
pinned genuine arm64 SDK in the owned diagnostic, and exercises a separate native
Cocoa GUI/engine/UDP lifecycle smoke. SDK-header/injected tests are software tests;
production Mac loader integration remains M3. No test is skipped to hide SIGPIPE.
If hosted capacity fails, report exact blocker and leave arm64 checks unrun.

**Milestones:** arm64 M0/M2 acceptance pending; protected Mac IPC remains open;
M1 physical NOT RUN; M3 real integration and M4/M5 packaging/hardware not delivered.
**Next exact task:** collect arm64 CI evidence (or its concrete capacity blocker),
record compiler/platform/SDK and GUI outcomes, then hand off remaining gates.

## Preserved Windows and planning history

# Session handoff — macOS port planning

Planning branch: `codex/handbridge-macos-plan`, based on handoff
`2c1ce8e72980c0ef4d59efa474e19df30c2666a2`.
Preserved Windows release branch: `codex/handbridge-leapc-release`.
Draft review: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/3.
Validated source: **`c8596f20b35bb0e4d09446933c543f689417f36c`**. Final handoff is docs/screenshots only.
Started at controller `3825ecd`; all prior branches and original ImGui source remain
recoverable. User authorized the adapter and package, then explicitly required
latest Hyperion v6. Do not regress to the older Unreal-plugin 5.30 research SDK.

## Reviewable code increments

- `5550359`: real SDK adapter/API table, safe loader/factory, owned native IDs,
  host/GUI startup, native-ID processing reset, genuine-SDK-type tests.
- `b21ffdf`: Release preset/deployment, Qt/QML/CRT/notices/source archives and smoke.
- `b142729`: Windows-native innoextract filters; no SDK hashes relaxed.
- `69c8f14`: genuine SDK offline host lifecycle in the extracted-package smoke.
- `c8596f2`: ignore generated JUnit evidence and require clean package provenance.

Changed modules: src/tracking/*; core/HandData; runtime TrackingSource/Processing;
both engine hosts; Controller/Main.qml; CMake SDK/control/release definitions;
SDK fetch/manifest/tests; Windows package/smoke/workflow; notices/screenshots/docs.
No OSC address/type/default or config schema changed. The original raw processor,
oscpack goldens and localhost:9000 defaults remain. Native IDs never enter legacy
addresses; known identity changes reset only the affected filter/gesture state.

## Exact SDK and dependency boundary

The repository contains no SDK header/import library/client DLL. The helper
verifies/extracts selected files without executing the official 6.2.0 installer:
Installer SHA-256: `e219438dc5031af3dc75e115f277c4e6e33475bad21fee1e90a278e77f5fadf1`.
Header SHA-256: `27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765`;
client DLL 6.2.0.0-c98d293a. Manifest pins installer/extractor/all selected files.
Client and ServerLibrary must each report 6.2.0 or newer 6.x via the real
LeapGetVersion API. No unknown/old version can establish healthy service state.
SDK/client/tracking service remain external, not part of the ZIP. Runtime paths
are absolute; CLI/env/installed Program Files discovery is in LEAPC_SETUP.md.

## Actual commands/results

Cloud native build (GCC 14.2 / Qt 6.8.3, task-local tools):

```sh
. work/qt-env.sh
cmake -S . -B out/build/leapc-control -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$PWD/work/qt/6.8.3/gcc_64;$PWD/work/qt-deps/usr" \
  -DHANDBRIDGE_BUILD_CONTROL=ON -DHANDBRIDGE_WITH_LEAPC=ON \
  -DLEAP_SDK_ROOT="$PWD/work/sdk-v6/extracted/app/LeapSDK"
cmake --build out/build/leapc-control --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ctest --test-dir out/build/leapc-control --output-on-failure
```

XDG cache/data/config/runtime paths were under work/. **82/82 PASS**, no skips,
20.48 s, including 21 genuine-header/injected-function lifecycle/version cases,
actual UDP goldens, hardware-preflight failure and independent GUI lifecycles.
ASan/UBSan project adapter/process cases:34/34 PASS, leak checking,0.59 s, no
third-party Qt/SDK instrumentation. SDK fetch helper:7/7 offline regressions.
SDK-free core and native-controller Windows/Ubuntu CI also passed at `c8596f2`.

Windows Release:

```powershell
python scripts/fetch-leapc-sdk.py
cmake --preset windows-release -DHANDBRIDGE_WITH_LEAPC=ON -DLEAP_SDK_ROOT="$PWD/work/leapc-sdk"
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
cmake --build --preset windows-release --target handbridge-portable
```

Qt prefix/build prerequisites and isolated smoke command: WINDOWS_RELEASE.md.
[Final Release run](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905):82 registered,81 PASS +1 expected POSIX-only skip,
32.03 s; three strict until-fail lifecycle repeats 19.09/19.08/19.07 s allPASS.
Actual SDK DLL/API load 6.2.0.0/6.2.0 passed. Real no-service host made 2 connection
attempts, reported handshake error/zero devices/zero OSC, and exited within the
15 s deadline. Extracted ZIP passed 16 native Windows checks with only Windows
system paths, including app-local module paths, actual PNG render, GUI close/
kill/reopen and continuing UDP. Independent download checked checksum/header hash,
source revision, clean manifest and SDK exclusion. Full evidence: reports/HYPERION6_RELEASE.md.

## Download / first local use

[Final portable preview](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905/artifacts/14187119668). GitHub artifact contains the inner app ZIP
and checksum; extract both layers and keep its DLLs/plugins/QML beside both EXEs.
No Qt/Python/VS developer install is needed to run it. Install official compatible
Hyperion 6.2 separately. Query the installed DLL before cameras:

```powershell
.\handbridge-engine.exe --check-leapc --leapc-library "C:\Program Files\Ultraleap\LeapSDK\lib\x64\LeapC.dll"
.\handbridge-control.exe
```

Start engine is real; Start simulator / --demo are explicit mocks. For a nonstandard
install path set HANDBRIDGE_LEAPC_LIBRARY before launching the GUI. A detached
engine survives window closure; foreground Ctrl-C or Task Manager stops it.

## Remaining physical Windows gate

Physical Windows 10/11 cameras/USB/service recovery, TouchDesigner receipt and soak
are **UNTESTED ON HARDWARE**. The offline SDK test did not install/run Hyperion's
service. Consumer Windows clean-machine/DPI/accessibility acceptance remains.
Global processing/channel settings and legacy optional-loss quirks are documented;
no fusion, per-channel overrides, v2/bundles, installer or full 3 D skeleton is added.

Open the preserved Windows branch in local Windows Codex and run HARDWARE_TEST_PLAN with two real
controllers and Hyperion 6.2. Record exact DLL/API/service versions, independent
streams, real TouchDesigner values, no hands, unplug/replug, service restart and
GUI kill/reopen. Record PASS/FAIL/BLOCKED in HARDWARE_TEST_RESULTS; diagnose only
observed failures and update handoff. Do not redesign the controller or claim the
original silent-stall incident solved from mock/no-service results.

## New planning task / single next Mac task — 2026-11-16

The user requested a macOS plan and asked where to start implementation. Added
MACOS_PORT_PLAN.md with M0 environment/compiler/SDK diagnostic, M1 two-device
physical feasibility, M2 native mocks/protected IPC/CI, M3 real engine, M4 native
bundle and M5 recovery/release acceptance. Linked it from the project spec, PLANS,
README and current state; refreshed the hybrid handoff. No application/build/test
code changed, no SDK was downloaded, and no Mac build or physical test ran.
Also repaired the formatting of the earlier Windows installer hash above.

Important constraints: official Hyperion 6.2 Mac downloads exist, but official
multi-device guidance has unresolved Windows-only wording. Qt 6.8's Mac target
floor is 12+, not Ultraleap's 11. Actual Apple C++ library support needs probes for
jthread/stop_token and atomic shared snapshots. Qt's Unix socket permission flags
are ignored on Mac, so the plan requires private endpoint directories/verification.

**Next:** start a fresh local Mac Codex chat on this plan branch and implement
M0 only using the exact prompt in MACOS_PORT_PLAN. Record actual machine/SDK data,
commands and PASS/FAIL/BLOCKED results; if no cameras, hand off the probe without
claiming M1. A new cloud chat does not provide physical Mac/USB access. Preserve
the existing GUI/Windows pipeline and do not implement the entire port at once.
