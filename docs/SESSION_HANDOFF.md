## M0 evidence follow-up — 2026-11-16

Final repeat 43958755065 completed: SDK download connection reset (curl 56),
diagnostic skipped, artifact upload ETIMEDOUT. These are infrastructure failures;
previous Intel SDK compile/load/no-service PASS remains valid. Corrected curl
retry-all-errors and immutable Actions pins; feature probes now opt-in because
Xcode 16.4 failures are established. One SDK-only retry is pending, then the user
has authorized the narrow M2 owned-thread/mutex-snapshot compatibility change.
No Hyperion downgrade, GUI redesign or physical acceptance is authorized/implied.

# Session handoff — macOS M0 automation

Branch: `codex/handbridge-macos-feasibility`, from planning `d104b87`.
Initial feasibility work committed/pushed as `f4f60a5`; compiler/SDK diagnostic and
runner harnesses through `cab1839`. Production Windows/GUI/loader/build/legacy
OSC/configuration remain unchanged; Hyperion 6.2+ within 6.x remains mandatory.

**M0 automated evidence delivered with blockers. M1 physical tests NOT RUN.
M2–M5 not started.** Actual results: [M0 report](reports/MACOS_M0.md).

| Scope | Actual result |
|---|---|
| arm64 Mac VM / macOS 15.8.2 / Xcode 16.4 / Clang 17 / libc++ 190102 / SDK 15.5 / CMake 4.4.5 | Both C++20 feature probes FAIL compilation; official Mac SDK signature/slices/exports inspected |
| Intel / macOS 15.7.9 / same Xcode/Clang/libc++/SDK / CMake 4.4.2 | Both feature probes FAIL; diagnostic compiled/linked; missing library/load-only/no-service PASS (exits 2/0/3) |
| Actual Intel client API | 6.2.0; all twelve exports resolved |
| Connected ServerLibrary | unavailable; no connection event during bounded 5 s run; zero devices/errors, clean exit |
| Corrected arm64 offline diagnostic | BLOCKED: GitHub explicitly failed runner acquisition after 15 minutes queued |
| Physical two-camera streams, serials, no-hands, USB/recovery, TD | NOT RUN on either architecture |

Changed files: `.github/workflows/macos-m0.yml`, isolated `probes/macos-m0/`
(threading/snapshot sources, bounded SDK-only client, independent feature/SDK
harnesses, commands), this state/handoff, the plan execution link and `docs/reports/MACOS_M0.md`. Both genuine
Mac packages have separately measured/pinned SHA-256 values and verified vendor
signature/notarization; no Windows SDK or Gemini substitution and no vendor files
committed/uploaded. The inspected canonical SDK avoids old control-panel plugins.

Commands/results: `python3 probes/macos-m0/run-features.py` records independent
compile failures (jthread/stop-token/callback absent, atomic shared_ptr specialization
absent); `python3 probes/macos-m0/run-sdk.py` builds/inspects and enforces bounded
real offline exit codes. Overall CI is correctly red because compiler features
failed, even when SDK checks pass. Detailed source/run IDs, commands, logs, hashes
and scope limits are in the report. Linux syntax/sanity checks, Python compilation
and git diff --check pass; no hardware claims follow. The existing core/controller/
Windows Release workflows passed at f4f60a5 and again at 1cd6b9b.

Final exact-source Intel repeat at cab1839 remains workflow-queued with no job
created as of 2026-11-16 13:32 America/Los_Angeles (run 43958755065); previous
native SDK result is at c9f337d. Latest device-allocation ownership correction was
compiled locally without warnings; native repeat will confirm it separately.

**Single next task:** finish remaining M0 evidence: collect the queued Intel
repeat 43958755065, then run the arm64 canonical diagnostic on the actual Mac or
an available arm64 runner, recording actual client/service versions. Retain compiler findings for M2's later compatibility ADR; do not rewrite
production or claim M1 until physical two-device observations exist.

---

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
