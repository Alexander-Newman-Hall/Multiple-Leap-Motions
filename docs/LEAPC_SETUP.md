# Real LeapC adapter: SDK and runtime setup

The new engine supports independent real LeapC devices when built with
`HANDBRIDGE_WITH_LEAPC=ON`. The adapter uses the exact supplied `LeapC.h` declarations,
not a replacement SDK header. It is separate from the preserved original ImGui app.
Physical camera/service/TouchDesigner behavior remains **UNTESTED ON HARDWARE**.

## Which SDK was actually used

This repository's original branches contain no `LeapC.h`, `.lib` or `.dll`.
The reproducible development baseline is extracted from the official **Hyperion
6.2.0 Windows installer**, the latest v6 download advertised on the official LMC2
page when checked on 2026-11-15. Its matching LeapC DLL is
**6.2.0.0-c98d293a**. Header SHA-256:
`27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765`.
The earlier Unreal-plugin5.30 research baseline was superseded at the user's
explicit v6 instruction. It is not used for the Release build or runtime policy.

[SDK provenance, licence evidence, official API references and hashes](reports/LEAPC_SDK_AUDIT.md)
are recorded separately. The helper fetches development files to ignored work/
with size/SHA-256 checks and atomic writes. It downloads the roughly630MB vendor
installer and a pinned extractor, extracts selected SDK files without executing
the installer, and verifies each extracted file. No SDK file is committed or placed in
the portable ZIP. Development retrieval is separate from binary redistribution.

## Build on Windows x64 without cameras

Install VS2022 C++ tools, CMake, Python for tests, and Qt6.8.3 MSVC2022 x64 for the
optional controller. Prefer the exact SDK header installed on your PC:

```powershell
$env:Path = "C:\Qt\6.8.3\msvc2022_64\bin;$env:Path"
cmake --preset windows-control -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
  -DHANDBRIDGE_WITH_LEAPC=ON -DLEAP_SDK_ROOT="C:/Program Files/Ultraleap/LeapSDK"
cmake --build --preset windows-control --parallel 2
ctest --preset windows-control
```

When no SDK is installed, the documented development baseline is available:

```powershell
python scripts/fetch-leapc-sdk.py
cmake --preset windows-control -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
  -DHANDBRIDGE_WITH_LEAPC=ON -DLEAP_SDK_ROOT="$PWD/work/leapc-sdk"
```

CMake prints the actual header path and SHA-256 on every configure. Changing
`LEAP_SDK_ROOT` refreshes discovery; a missing header fails configuration. Native
Windows hardware loading requires x64. Linux can compile the real header/adapter
and run injected-API tests, but its actual runtime loader is deliberately unsupported.
Without the option, the mock build works and real startup fails with a clear error.

## Verify and select the installed runtime

Install a compatible official Ultraleap Hyperion runtime separately. The
application loads an **absolute x64 DLL path** using Windows safe DLL search flags;
it does not search the working directory or developer PATH for a bare LeapC.dll.
Selection order:

1. `--leapc-library <absolute path>`.
2. `HANDBRIDGE_LEAPC_LIBRARY` environment variable (absolute DLL path).
3. `LEAP_SDK_ROOT/lib/x64/LeapC.dll` when that environment variable is set.
4. Windows Program Files known folder + `Ultraleap/LeapSDK/lib/x64/LeapC.dll`.

```powershell
.\handbridge-engine.exe --check-leapc --leapc-library "C:\actual\SDK\lib\x64\LeapC.dll"
.\handbridge-engine.exe --headless --leapc-library "C:\actual\SDK\lib\x64\LeapC.dll"
.\handbridge-control.exe
```

The check loads the DLL, resolves all twelve required API exports, reports its
Windows file version and actual `LeapGetVersion(ClientLibrary)` version, then
unloads it. The client must report **6.2.0 or newer6.x**. It opens no LeapC connection, creates no
profile state and needs no service/camera. Missing, relative, wrong-architecture
and incompatible libraries must fail clearly. It proves loading/API availability,
not compatibility with a running service or physical camera.

For GUI startup with a nonstandard install path, set the environment before
launching the controller:

```powershell
$env:HANDBRIDGE_LEAPC_LIBRARY = 'C:\actual\SDK\lib\x64\LeapC.dll'
.\handbridge-control.exe
```

**Start engine** checks the runtime asynchronously then starts a detached real
engine. **Start simulator** selects mocks. Service connectivity is acknowledged
only by an SDK connection event followed by a successful
`LeapGetVersion(ServerLibrary)` check requiring **6.2.0 or newer6.x**. Unknown,
older and other-major versions are rejected before device subscriptions. The GUI
reports the actual service version separately from the DLL version, loaded library identity,
connection attempts, poll errors and retained tracking errors. Closing/crashing
the GUI still leaves the engine alive; foreground Ctrl-C or Task Manager stops it.

## Hardware gate

The adapter uses one poll owner, multi-device-aware connection, explicit per-device
subscriptions, owned raw-mm observations and device-local native hand IDs. Empty
hand frames count as healthy tracking. Handshake timeout, per-device stale restart
and service reconnection use bounded work/backoff. These paths have injected SDK
tests; actual USB/service timing is still unverified.

Run [HARDWARE_TEST_PLAN](HARDWARE_TEST_PLAN.md) on Windows, record the exact
installed SDK/runtime versions and [results](HARDWARE_TEST_RESULTS.md), and verify
actual TouchDesigner receipt. A successful DLL check or fake frame test never
completes that gate.
