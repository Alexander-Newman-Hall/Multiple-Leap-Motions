# Dependencies, build targets and licensing ledger

## Current build inputs

The native engine, optional Qt controller and real LeapC adapter are implemented.
SDK-free builds remain supported; simulation requires explicit `--simulate`.
Hardware operation is **UNTESTED ON HARDWARE**. See [CURRENT_STATE](CURRENT_STATE.md)
for the current commit and CI results, [BUILD_AUDIT](reports/BUILD_AUDIT.md) for the
historical build audit, and [GUI_BUILD](reports/GUI_BUILD.md) for controller evidence.

| Component | Purpose | Version / boundary |
|---|---|---|
| Windows 10/11 x64 | Target runtime | Exact supported OS builds require local acceptance. |
| Apple Silicon / macOS arm64 | Mac extension target | Native arm64 only; no Intel, Rosetta or universal-release requirement. Acceptance requires actual arm64 results; historical Intel checks do not transfer. |
| MSVC 2022 + Windows SDK | Windows C++20 build | x64 Visual Studio 17 2022 presets; Release uses dynamic MSVC runtime. |
| CMake / Ninja | Build | CMake 3.24+; CI pins CMake 3.31.6 and Ninja wheel 1.11.1.4. |
| Ultraleap Hyperion / LeapC | Actual tracking provider | Official Hyperion **6.2.0** SDK; client DLL **6.2.0.0-c98d293a**. External runtime/service; never bundled in this preview. |
| Qt Core, Network | Local IPC host and controller model | Official Qt **6.8.3**; engine links no Qt GUI, Quick or QML. |
| Qt Quick/QML | Optional native GUI | Official Qt **6.8.3**; separate process. QtTest is test-only. |
| oscpack | Legacy OSC/UDP | Commit `3e7e3a102e316d75fe176885a6dd08f216f2eac0`; MIT. |
| nlohmann/json | Configuration and data serialization | **3.11.3**, commit `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`; MIT. |
| GoogleTest | C++ unit and injected-API tests | **1.15.2**; BSD-3-Clause; not deployed. |
| Python 3.9+ | Test receiver and development helpers | Build/test/fetch tools only; no Python engine or runtime dependency. |
| PowerShell 7+ | Windows Release packaging/smoke | Packaging only; not required for ordinary Debug builds or deployed applications. |
| TouchDesigner | OSC receiver | Separate installation; actual receipt requires Windows receiver verification. |

Open-source CMake fetches use exact revisions and SHA-256 checks in ignored build
directories. CMake does not automatically download the proprietary SDK.

## Genuine Mac Hyperion 6.2 SDK — native arm64 only

Mac validation uses the official **Apple Silicon 6.2.0** package and its actual
arm64 header/dylib, not files extracted from the Windows installer below. The
package SHA-256 is
`9cad82a6282e322d2acd71f3746e50b472f3ed83bb5f9fa6f54d554f404c5531`;
provenance/inspection and unexecuted checks are recorded in [M0 evidence](reports/MACOS_M0.md).
No SDK file is committed or redistributed. Qt remains **6.8.3**, C++ remains **20**,
and client/connected ServerLibrary must independently meet **6.2.0 or newer 6.x**.
Use `macos-arm64-core` / `macos-arm64-control` presets and arm64 CI. If a dependency
archive contains other architecture slices, validate and execute arm64; its archive
format does not impose a universal application release requirement. Historical
Intel results are retained as historical evidence only. Capacity failure is BLOCKED,
not SDK incompatibility or permission to substitute an Intel build. A Mac SDK load
without a running service establishes no physical camera/recovery/TD acceptance.

## Genuine Windows Hyperion 6.2 development SDK

Use the exact installed SDK when available, or explicitly run:

```sh
python scripts/fetch-leapc-sdk.py
# LEAP_SDK_ROOT=<repository>/work/leapc-sdk
```

The helper retrieves the official `tracking-software-windows-6.2.0.exe`, verifies
its size/SHA-256 and a pinned Innoextract 1.9 archive, then extracts and verifies
selected SDK files. It does not run the installer or install a tracking service.
`--dest`, `--cache-dir` and `--header-only` are supported. Even a first header-only
retrieval downloads the approximately 630 MB installer. SDK/cache files stay under
ignored `work/`; neither SDK binaries nor headers are committed or published.

[cmake/leapc-sdk-baseline.json](../cmake/leapc-sdk-baseline.json) records exact
official URLs, versions, file sizes and digests. The genuine 6.2 `LeapC.h` hash is
`27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765`.
The former Unreal-plugin 5.30 research input is superseded and is not the supported
build/release baseline. See [LEAPC_SDK_AUDIT](reports/LEAPC_SDK_AUDIT.md) for provenance,
official API references and licence evidence.

Enable the adapter with `HANDBRIDGE_WITH_LEAPC=ON` and an explicit `LEAP_SDK_ROOT`.
CMake validates the actual header's version API and prints its SHA-256. Linux can
compile the real adapter against this header and test injected SDK function
pointers without an SDK binary; the production runtime loader currently requires
Windows x64. The separate Mac SDK-only diagnostic has its own native arm64 acceptance gate; it is not production
Mac engine integration.
The runtime checks the client and connected server-library versions independently;
each must report 6.2.0 or newer within 6.x. A successful header compile, DLL
preflight or injected-API test does not prove service/camera operation.

`Qt6_DIR` or `CMAKE_PREFIX_PATH` selects the installed Qt. See
[LEAPC_SETUP](LEAPC_SETUP.md) for hardware builds and
[WINDOWS_RELEASE](WINDOWS_RELEASE.md) for exact Release/package commands. The
`windows-release` preset keeps the SDK off unless explicitly enabled.

## Actual targets

- `handbridge-core`: retained configuration, alias, sorter, processor and OSC code.
- `handbridge-runtime`: validated settings, processing, mock source and supervision.
- `handbridge-tracking`: provider factory; real adapter/runtime loader when SDK-enabled.
- `handbridge-engine`: independent console host; Qt Core/Network IPC host when
  `HANDBRIDGE_BUILD_CONTROL=ON`, otherwise Qt-free. Real provider is the default;
  SDK-off real mode reports an actionable error. `--simulate` selects mock tracking.
- `handbridge-control-client` / `handbridge-control`: optional C++ controller model
  and Qt Quick GUI, with no ownership of an existing engine's lifetime.
- `handbridge-portable`: opt-in Windows Release deployment target.
- `leapMotionApp`: preserved original coupled GUI, opt-in legacy target; not the
  modern controller or a hardware-verified fallback.

## Licensing and distribution

The portable preview dynamically deploys Qt 6.8.3, required QML/plugins and
app-local Microsoft **Release** CRT files from the VS2022 redistributable
directory. It includes dependency notices, applicable Qt LGPL/GPL/exception text,
and exact qtbase/qtdeclarative corresponding-source archives in the ZIP. Compatible
Qt library replacement is permitted; manifest hashes are diagnostic. See
[packaging notices](../packaging/THIRD_PARTY_NOTICES.txt) and
[Qt source manifest](../packaging/qt-sources.json).

The package excludes LeapC.dll, SDK headers/import libraries, Hyperion service,
camera firmware, debug CRT/Qt libraries and build tools. Users obtain Ultraleap's
runtime separately under its terms. Development retrieval is not permission to
publish SDK-only archives. The application repository has no general open-source
licence; dependency licences do not grant additional rights to application code.
Never store signing keys, tokens or private camera identifiers in source control.

Current Linux evidence is 82 SDK-enabled CTest entries passed, including 21 tests
using genuine SDK types and injected functions. Windows Release actual 6.2 DLL
load/version/API checks and extracted-package execution without a developer PATH
have now independently passed in the Release workflow at c8596f2. The package
records a clean source checkout; actual SDK/client6.2.0.0 startup without a service
and all16 portable checks passed. Physical USB/Hyperion/TouchDesigner
acceptance remains separate; see [TESTING](TESTING.md).
