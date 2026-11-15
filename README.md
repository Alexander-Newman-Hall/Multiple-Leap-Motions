# Hand Bridge 2.0

Native C++20 multi-camera Ultraleap → OSC/UDP → TouchDesigner on the same
Windows PC. Independent camera streams; legacy OSC remains the default.

**Current deliverable: Phase 0 audit and a Phase 1 headless simulation foundation.**
The new engine requires `--simulate`; its LeapC hardware adapter and optional Qt
controller are not implemented. Real cameras, Hyperion recovery, Windows USB and
TouchDesigner receipt are **UNTESTED ON HARDWARE**. The original Windows source is
preserved; `master` and `handbridge-v2` are unchanged. Read the
[original README](docs/legacy/README.md) for historical implementation notes.

## Build and test without an SDK

Requires Git/network for pinned open-source dependencies, CMake 3.24+, a C++20
compiler, and Python 3.9+ for the process-level UDP test. Linux additionally uses
Ninja. No proprietary SDK, Qt, SDL or OpenGL is needed for this path.

Windows x64, Visual Studio 2022 with Desktop development with C++:

```powershell
cmake --preset windows-core
cmake --build --preset windows-core --parallel 2
ctest --preset windows-core
.\out\build\windows-core\Debug\handbridge-engine.exe --simulate --headless --duration-ms 5000
```

Linux:

```sh
cmake --preset core-debug
cmake --build --preset core-debug --parallel 2
ctest --preset core-debug
./out/build/core-debug/handbridge-engine --simulate --headless --duration-ms 5000
```

These commands build the **simulated** engine and real OSC serializer/UDP output.
A successful cloud run does not establish Windows/hardware support. CI has
separate Windows/Linux SDK-free jobs and publishes test results, not a hardware
product executable. See [current evidence](docs/CURRENT_STATE.md).

## Run the simulated engine

```sh
handbridge-engine --simulate --headless --host 127.0.0.1 --port 9000 --scenario stall --duration-ms 5000
handbridge-engine --simulate --config configs/legacy-default.example.json
```

Use the executable path from the build commands. Without `--duration-ms` the
process runs until Ctrl-C. `--scenario stall` pauses simulated camera A at
200–900 ms; B continues, A becomes stale and retries, then resumes. Logs record
state transitions and a final JSON summary reports simulated state/send counts.
No GUI object/event loop is linked into this executable. No GUI attach/restart
behavior is claimed until a controller exists.

Default destination: **127.0.0.1:9000**. Saved valid ports, including historical
7000, are preserved. Set TouchDesigner's OSC In DAT/CHOP to the exact chosen
port; first inspect `/leap/dev1/left/palm/tx` and `/leap/dev2/right/palm/tx`.
Coordinates are raw millimeters, one float32 per message/datagram. A successful
OS send is not receiver acknowledgement. See [OSC protocol](docs/OSC_PROTOCOL.md)
and [TouchDesigner setup](docs/TOUCHDESIGNER_SETUP.md).

`--config` currently accepts **legacy JSON only**, reads without saving, and
rejects invalid/unsupported versioned configurations. The proposed
`configs/handbridge.example.json` is a future schema and is deliberately rejected.
See [implemented config contract](docs/CONFIG_SCHEMA.md). No implicit migration
or automatic rewriting of an installation's config occurs in the new host.

## Windows SDK application reference

Install the official Ultraleap SDK/Hyperion under its license, then set
`LEAP_SDK_ROOT` to the directory containing `include/LeapC.h` and
`lib/x64/LeapC.lib`:

```powershell
$env:LEAP_SDK_ROOT = 'C:\path\to\installed\LeapSDK'
cmake --preset windows-legacy
cmake --build --preset windows-legacy --parallel 2
```

This optional target is the **original coupled ImGui application**, with repaired
source selection and explicit SDK diagnostics. It is not the new headless LeapC
adapter and has not been compiled in this cloud environment. Do not treat it as a
validated Windows build; follow [dependencies](docs/DEPENDENCIES.md),
[build audit](docs/reports/BUILD_AUDIT.md) and [hardware plan](docs/HARDWARE_TEST_PLAN.md).
No SDK binaries are downloaded, committed or redistributed.

## Project documents

- [Authoritative specification](PROJECT_SPEC.md), [phased plan](PLANS.md), [agent instructions](AGENTS.md)
- [Code audit](docs/LEGACY_AUDIT.md), [architecture](docs/ARCHITECTURE.md), [decisions](docs/DECISIONS.md)
- [Reliability](docs/RELIABILITY.md), [testing](docs/TESTING.md), [operations](docs/OPERATIONS_RUNBOOK.md)
- [Current state](docs/CURRENT_STATE.md), [session handoff / next task](docs/SESSION_HANDOFF.md), [continue on your Windows PC](docs/WINDOWS_LOCAL_HANDOFF.md)
- [Cloud ↔ Windows workflow](docs/HYBRID_WORKFLOW.md), [physical results](docs/HARDWARE_TEST_RESULTS.md)

Do not build into the historically tracked `build/` directory. New generated
files/dependencies go under ignored `out/` and task scratch files under `work/`.
