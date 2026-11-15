# Dependencies, build targets and licensing ledger

**Status:** Draft. Codex must reconcile this with the installed SDK, original lockfiles and the actual Windows CI configuration before pinning versions.

| Component | Purpose | Version / license / action |
|---|---|---|
| Windows 10/11 x64 | Target runtime | Validate exact minimum and supported OS builds on test PC. |
| MSVC + Windows SDK | C++20 build | Pin supported toolchain in `CMakePresets.json`/CI. |
| CMake + Ninja or VS generator | Reproducible build | Pin minimum version supported by chosen dependencies. |
| Ultraleap Hyperion LeapC | Native tracking data and multi-device subscriptions | Validate installed SDK headers, service version, supported devices and redistribution rules. Never vendor binaries without permission. |
| Qt 6 Core, Network | Headless host IPC only | Use `QLocalServer`/`QLocalSocket`; no GUI/Quick in headless target. Validate Qt licensing and packaging requirements. |
| Qt 6 Quick/QML | Optional native GUI | Package only with GUI executable; support disconnected engine mode. |
| OSC library (`oscpack` or equivalent) | OSC packet serialization | Retain compatibility with original oscpack if feasible. Pin source revision and license. |
| JSON library (`nlohmann/json` or equivalent) | Versioned config, IPC | Pin revision and schema tests. |
| Test framework (`GoogleTest`, `Catch2` etc.) | Unit/integration | Use one coherent framework (current CMake/test sources may disagree). |
| GitHub Actions Windows runner | CI smoke/unit builds | Can build mock targets; SDK-integrated path depends on legally available binaries. |
| TouchDesigner | OSC receiver | Physical Windows validation only, not a cloud CI prerequisite. |

## Build target classes

1. `handbridge-domain` / processing / osc: build on a standard C++ runner without LeapC or Qt UI.
2. `handbridge-engine-mock`: runnable with simulated devices, no proprietary SDK, no Qt GUI.
3. `handbridge-engine` hardware target: requires LeapC dev library and working Hyperion installation at runtime; compile on Windows with SDK.
4. `handbridge-control`: optional Qt Quick/QML executable; connects via local IPC.
5. `handbridge-tests`: fake-source, serializer, configuration and lifecycle tests.

## Environment configuration (proposed)

- `LEAP_SDK_ROOT`: explicitly points to installed SDK when building the real adapter.
- `Qt6_DIR` or `CMAKE_PREFIX_PATH`: selects installed Qt version.
- `CMAKE_BUILD_TYPE` or MSVC configuration: use standard Debug/Release; avoid introducing absolute developer machine paths.

Codex must write **real tested CMake configure/build commands** into the root README once targets exist. Do not pretend targets mentioned in this planning doc already compile.

## Licensing / distribution

- Verify Hyperion SDK redistribution and runtime installer requirements directly from current license terms; do not assume permission to commit DLLs.
- Qt has different licensing options; review compliance, linkage, attribution, and release distribution. Native Qt Quick/QML is not automatically free of redistribution obligations.
- Track and preserve licenses/notices for all dependencies; supply them in distribution.
- Do not store tokens, signing keys, licenses or user-private hardware identifiers in source control.
