# Build and test inventory — Phase 0

Observed 2026-11-15 from `handbridge-v2` commit `5f547181f6d512ab6a8d2146022227e26cec48d2`. Original `master` remains `054e578525a5d5202ea8f6eb417ce55ded7d61b8`. Line references below target the preserved original definition at `cmake/legacy-original.cmake`, byte-identical to baseline `CMakeLists.txt`.

## Confirmed baseline build defects and coupling

| Evidence | Consequence | Decision |
|---|---|---|
| Original CMake:5–11,102,183–190 | Machine-specific LeapC and oscpack paths; Win32 oscpack backend unconditional. | Replace build plumbing with SDK-free default; explicit local SDK root only for optional Windows legacy target. |
| Original CMake:25; `.vcxproj`:101,117,133,150,176 | Both specify C++17. | CMake now requires C++20; original VS project retained as historical evidence, not current build authority. |
| Original CMake:79–99,142–176 | Even processing unit tests require SDL/OpenGL/LeapC because all are in one library. | Split preserved portable processing/config/sender from SDK/GUI sources. |
| Original CMake:129–149,201–210 | Omits `src/app/AppCore.cpp` and `src/ui/UIController.cpp` used by main; includes dead `UIManager.cpp` (which has missing JSON path and duplicate shutdown). | Optional legacy target explicitly lists the live VS application path, excludes dead UIManager/duplicate wrappers. |
| Original CMake:140 | Appends a directory as if it were a source. | Remove from new target lists. |
| Original CMake:120–126 and checked source bare `json.hpp` imports | `third_party/` does not exist; no nlohmann/json fetch or target in original CMake. | Pin nlohmann/json and publish its bare-header include path for legacy source compatibility. |
| Original CMake:226–252 | Only `test_DataProcessor` gets `add_test`; glob creates other executables without CTest registration. `AspectMapper` deliberately excluded despite fetching Catch2. | Explicit reviewed test targets with GoogleTest discovery; no mixed test frameworks for new work. |
| `.vcxproj`:66–97 | Imports `../build/props/{leap_sdk,sdl2,glew,oscpack,imgui}.props`; none exists in checkout. | Use generated MSVC project from presets. |
| `.vcxproj` source list and resource properties | References missing headers (`FilteredFrameData.hpp`, `RawFrameData.hpp`, etc.) and `D:\Desktop\...` resource include. | Preserve project for rollback, stop advertising it as reproducible build. |
| `pipeline/00_LeapConnection.cpp`:4; `01_LeapPoller.cpp`:120 in baseline | Relative `../../LeapSDK/include/LeapC.h` bypasses SDK include path. | Root repairs include spelling only; actual SDK API compilation remains unverified. |
| `git ls-files build` | 614 tracked generated files, 7.5 MiB; generated cache records machine-specific locations. | Leave tracked artifacts untouched this task; ignore fresh `/out`, `/work`, `/build` output. Separate cleanup after baseline acceptance. |

The Visual Studio live source list includes AppCore/UIController but also obsolete `LeapConnectionImpl`/`LeapDeviceManager` paths; CMake differed materially. New `leapMotionApp` is an **optional legacy GUI target**, not the new SDK-backed headless adapter. Original GUI/SDK compilation is NOT RUN; the new SDK-free Windows host is separately validated below.

## Tests retained and tests deferred

All original test files are preserved. The default CTest suite registers repaired `test_ConfigManager`, `test_DeviceAliasManager`, existing `test_LeapSorter`, and new `test_LegacyOscContract`, `test_EngineRuntime`, plus the Python standard-library-only process/UDP receiver integration test. Python is test infrastructure, not the engine.

| Original test | Audit outcome |
|---|---|
| `test_ConfigManager.cpp` | Original called nonexistent `saveConfig(filename)` / `loadConfig` assumptions; repaired to actual `save/load` API with defaults/alias/migration/rejection regressions. |
| `test_DeviceAliasManager.cpp` | Retained and expanded with persistence/collision/invalid-input coverage. |
| `test_LeapSorter.cpp` | Retained actual source test; does not prove concurrent UI writes safe. |
| `test_DataProcessor.cpp` | Accesses private filter fields and removed mapping/gain/visualization methods; stale. New fixtures test actual current raw-mm processor. |
| `test_DataProcessor_HandPresence.cpp` | Calls nonexistent `getVizState()`, assumes removed normalized-coordinate feature. Replaced coverage by actual output/loss fixtures, original retained. |
| `test_PipelineEndToEnd.cpp`:80–87 | Expects serial strings in OSC addresses although implementation uses `devN` aliases. New two-device loopback test asserts actual address contract. |
| `test_OscSender.cpp` | Fixed ports, receiver startup race, shared string unsynchronized, missing POSIX `fcntl.h`, expects unimplemented bundle. New receiver binds an ephemeral port before sending and decodes OSC float bytes. |
| `test_LeapConnection.cpp` | Requires real SDK/service; not a hardware-free test. Deferred Windows adapter work. |
| `test_LeapPoller.cpp` | Tests only a local vector-based fake class, does not include application poller. Preserved but not counted as LeapC coverage. |
| `test_MainAppWindow.cpp` | Windows/SDL UI dependency, partly commented assertions. Deferred replacement UI work. |
| `test_DeviceTrackingModel.cpp` | Obsolete UI-only model; `getDevice` returns mutable reference after unlocking. Deferred UI work, not new runtime authority. |
| `test_AspectMapper.cpp` | Catch2-only abandoned normalized mapper; not on active raw-mm path. Deferred processing work. |
| `TestLeapDataRouter.cpp` | Includes nonexistent `src/core/LeapDataRouter.hpp`. Preserved as stale historical test. |

## Dependency pins and licensing

SDK-free CMake downloads only source archives, pinned to exact Git SHA and SHA-256 digest:

- nlohmann/json v3.11.3, `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`, MIT (`LICENSE.MIT`).
- oscpack `3e7e3a102e316d75fe176885a6dd08f216f2eac0`, MIT (`LICENSE`). Serializer retained. Native checked UDP sender uses OS sockets because oscpack's Send ignores sendto's return value.
- GoogleTest v1.15.2, `b514bdc898e2951020cbdca1304b75f5950d1f59`, BSD-3-Clause (`LICENSE`), tests only.

Optional legacy GUI fetches exact original versions at immutable commits: GLM 0.9.9.8 (`bf71a834948186f4097caa076cd2663c69a10e1e`, MIT), SDL 2.26.5 (`ac13ca9ab691e13e8eebe9684740ddcb0d716203`, zlib), ImGui 1.89.9 (`c6e0284ac58b3f205c95365478888f7b53b077e2`, MIT). Qt is not introduced yet. No SDK headers/libraries/binaries are downloaded, vendored or redistributed. SDK and Qt release terms remain a later packaging gate; CI uploads only test reports, not product binaries. Preserve dependency notices when packaging.

## Real environment and baseline commands

CLOUD: Debian GNU/Linux 13 x64, GCC 14.2.0, Python 3.12.14. No CMake/Ninja initially (`cmake: command not found`). Installed user-local tools without system modification:

```sh
python3 -m venv work/tooling
work/tooling/bin/python -m pip install cmake==3.31.6 ninja==1.11.1.4
```

Verified CMake 3.31.6, Ninja `1.11.1.git.kitware.jobserver-1` (wheel 1.11.1.4).

Before source build edits, ran original online clean configuration:

```sh
work/tooling/bin/cmake -S . -B work/baseline-online
```

**FAIL (expected documented baseline blocker):** `Could NOT find OpenGL (missing: OPENGL_opengl_LIBRARY OPENGL_glx_LIBRARY OPENGL_INCLUDE_DIR)` at original `CMakeLists.txt:93`; SDK-independent test configuration therefore impossible. Earlier explicitly disconnected configure failed at same dependency (with FetchContent warnings about absent downloads). Full legacy compile/test was not reached. Raw cloud-only logs remain ignored under `work/`.

Current portable build commands:

```sh
# Normal installations with CMake/Ninja on PATH:
cmake --preset core-debug
cmake --build --preset core-debug --parallel 2
ctest --preset core-debug
# This cloud environment's local tools:
work/tooling/bin/cmake --preset core-debug -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja"
work/tooling/bin/cmake --build --preset core-debug --parallel 2
work/tooling/bin/ctest --preset core-debug
```

Latest passing results and exact commit are recorded centrally in `CURRENT_STATE.md` / `SESSION_HANDOFF.md` after integration. Windows CI is configured for Windows Server 2022 / VS2022 x64 and Ubuntu 24.04, with pinned CMake/Ninja/Actions revisions. **Windows/Ubuntu SDK-free workflow execution now observed; final evidence below and in CURRENT_STATE.** It builds only the SDK-free simulated engine.

Windows local SDK path (PowerShell in VS2022 development environment):

```powershell
cmake --preset windows-core
cmake --build --preset windows-core
ctest --preset windows-core
$env:LEAP_SDK_ROOT = 'C:/path/to/installed/Ultraleap/LeapSDK'
cmake --preset windows-legacy
cmake --build --preset windows-legacy
```

The optional SDK target diagnoses missing headers/import library clearly. It does not install runtime software or validate USB/service/TouchDesigner behavior. Actual SDK symbols, installed layout, service compatibility and hardware behavior require Windows verification.

## Integrated cloud verification

Before implementation commits (working branch `codex/handbridge-phase0-reliability`, parent audit commit `350249f`):

- Debug configure/build: PASS; initial CTest 37/37 before final broadcast-send regression was added.
- Release configure/build: PASS; CTest **38/38**, zero failures, 3.70 s. Uses preset commands above with `core-release` and JUnit file `work/core-release-results.xml`.
- AddressSanitizer + UndefinedBehaviorSanitizer Debug configure/build: PASS; CTest **38/38**, zero failures, 4.07 s. Project core/runtime/sender/tests are instrumented; third-party oscpack/GTest are not instrumented. No sanitizer diagnostics observed. Actual commands (reuses already verified source downloads):

```sh
work/tooling/bin/cmake -S . -B out/build/core-sanitized -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja" \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DHANDBRIDGE_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$PWD/out/build/core-debug/_deps/nlohmann_json-src" \
  -DFETCHCONTENT_SOURCE_DIR_OSCPACK_SOURCE="$PWD/out/build/core-debug/_deps/oscpack_source-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/out/build/core-debug/_deps/googletest-src"
work/tooling/bin/cmake --build out/build/core-sanitized --parallel 2
work/tooling/bin/ctest --test-dir out/build/core-sanitized --output-on-failure \
  --output-junit "$PWD/work/core-sanitized-results.xml"
```

- `HANDBRIDGE_BUILD_LEGACY_WINDOWS=ON` on Linux: expected configure rejection, `The original LeapC application requires Windows x64; use an SDK-free core preset here.` This checks platform diagnostics only.
- Static syntax checks of preserved stale tests actually run with GCC14/C++20 and fetched JSON/GTest headers: `test_DataProcessor.cpp` rejected private `sendPalm_` access; `test_DataProcessor_HandPresence.cpp` rejected nonexistent `getVizState`; `TestLeapDataRouter.cpp` failed missing header. Logs ignored under `work/*.syntax.log`. These failures are the recorded baseline, not hidden passing tests.
- GitHub Actions checkout/setup-python/upload-artifact SHA pins verified against tags using `git ls-remote`; initial workflow execution was pending at that checkpoint; observed results follow below.

Original SDK application, Hyperion SDK/service, physical USB devices, TouchDesigner receipt and hardware soak remain **NOT RUN / UNTESTED ON HARDWARE**. SDK-free Windows compiler results are recorded below.

## Final integration rerun

After final runtime health/timestamp regressions and UDP loss-packet test changes,
root rebuilt the frozen code tree on 2026-11-15: Debug **40/40 PASS (3.76 s)**,
Release **40/40 PASS (3.73 s)**, ASan+UBSan **40/40 PASS (4.11 s)**, no skips or
sanitizer diagnostics. These supersede the earlier 37/38-case checkpoints above.
Exact tested source SHA and observed workflow status are recorded in the final
SESSION_HANDOFF/CURRENT_STATE documentation checkpoint.

First observed PR CI run43766927335: Ubuntu configure/build/test PASS40/40;
Windows Server2022/MSVC19.44 configure/build PASS, tests39 passed/1 failed due to
a POSIX-only broadcast-denial assumption. Correction a18132b explicitly skips that
OS-specific test on Windows; no production code changed. JUnit output now uses an
absolute workspace path so both XML and LastTest.log are retained by the artifact
step (relative CTest output was placed under the binary directory). Final CI run
and exact source SHA are recorded in CURRENT_STATE/SESSION_HANDOFF.

Observed final source/test CI at a18132b, run43767212081: Ubuntu job PASS40/40;
Windows Server2022 / MSVC19.44 configure/build PASS,39 passed +1 explicit POSIX-only
skip,0 failures,4.30s. Kernel permission denial is only validated on Linux; Windows
UDP/closed-socket/readiness/config/runtime/process tests remain active. Run43767314985
for the XML path-only change61ca882 had Ubuntu PASS and Windows in progress at
handoff. No actual hardware/SDK/TouchDesigner test is implied by these results.
