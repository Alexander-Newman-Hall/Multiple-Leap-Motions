# Session handoff — Hyperion 6 adapter / Windows Release preview

Task branch: `codex/handbridge-leapc-release`.
Starting commit: `3825ecd0d7acf35be6a8a7d164ebef01bd25d790` on preserved
codex/handbridge-native-controller. This checkpoint accompanies the adapter
commit (`5550359`); final package source SHA and Windows workflow evidence follow after CI.
Original master054e578, handbridge-v2 5f54718 and previous feature branches are
preserved. User explicitly authorized both SDK integration and portable packaging,
then clarified that latest Hyperionv6 is mandatory.

## Changed components

- src/tracking/: genuine-header API table, real adapter, safe Windows loader,
  factory, explicit mock/real selection and nontracking runtime preflight.
- src/runtime/TrackingSource.hpp and core/HandData.hpp: diagnostics + optional
  device-local native hand ID; no cross-camera identity/fusion.
- Processing.cpp/.hpp: reset filters/gesture state on known native-ID changes.
- Both engine hosts: default real mode, --simulate explicit, --leapc-library,
  --check-leapc, actual provider/library/API/service versions and counters.
- Controller.cpp/.hpp + Main.qml: async hardware preflight/detached startup,
  separate simulator, truthful version/error panels, independent lifetime.
- CMakeLists/LeapC.cmake/NativeControl.cmake: optional real SDK-header target and
  actual SDK typed tests; SDK-off builds remain supported.
- SDK manifest/fetch script/tests/report: official Hyperion6.2 extraction with
  pinned sizes/SHA-256 and no SDK publication.
- Windows release preset/deploy scripts/workflow/packaging: both apps, Qt/QML,
  app-local Release CRT, notices/corresponding source, checksum manifest and
  extracted-package PATH-clean GUI/UDP/lifecycle smoke.

## SDK and compatibility

The repo itself has no SDK files. Current baseline is selected files extracted
without installation from official tracking-software-windows-6.2.0.exe:
installer SHA256e219438dc5031af3dc75e115f277c4e6e33475bad21fee1e90a278e77f5fadf1;
LeapC.dll file version6.2.0.0; SDK header
27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765.
Unreal-plugin5.30 files were research only and are superseded.
Client and connected ServerLibrary API versions must be6.2.0 or newer6.x.
LeapGetVersion is taken from the exact real6.2 header; unknown/older versions are
rejected before healthy service/device enumeration. Hardware verification remains
false in runtime telemetry until a separate physical acceptance report.

No OSC address/type/default or persistent configuration version changed. Native
IDs do not enter legacy addresses. Empty-hand delivery is liveness. The legacy
optional orientation/velocity/normal loss quirks remain documented in OSC_PROTOCOL.
SDK client/Hyperion service are external prerequisites, not ZIP contents.

## Commands and actual results

Cloud Qt/core source build uses GCC14.2, Qt6.8.3 and task-local tools/dependencies:

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

XDG cache/data/config/runtime directories are set under ignored work/ during the
run. Result: **82/82 PASS, zero skips,20.48s**. This includes21 real-SDK-type
injected lifecycle/version tests, raw conversion/ownership, native-ID regression,
actual UDP goldens and real GUI/process tests. SDK-off native suite also passed
61/61 before the additive final service-version fields. SDK-off host/preflight
manual checks rejected real startup clearly and created no profile state.

Windows commands and package location: WINDOWS_RELEASE.md. SDK/runtime selection:
LEAPC_SETUP.md. Windows real-DLL/portable gate is pending its first workflow run;
PowerShell syntax, helper argument/Unicode/PNG logic and Qt source checksums have
passed locally. These checks do not establish a working Windows ZIP yet.

## Boundaries / single next task

Physical Windows Hyperion6.2, two USB cameras, actual service restart/unplug,
TouchDesigner receipt and hardware soak remain UNTESTED ON HARDWARE. A Windows
CI DLL-load/PATH-clean smoke is not a clean consumer machine or physical test.
Do not redesign the GUI, add fusion, or claim the silent-stall incident solved.

After cloud deployment checks pass, open this branch on the local Windows PC and
execute the Hyperion6.2 two-camera acceptance checklist. Record exact DLL/API /
service versions, two independent streams, real OSC receipt, unplug/replug,
service restart and GUI closed/reopened behavior in HARDWARE_TEST_RESULTS.md.
Fix only observed failures in a small follow-up; record blocked tests truthfully.
