# Hand Bridge 2.0.1

A native C++20 tracking bridge and modern Qt6 controller for independent Ultraleap
camera streams over OSC/UDP to a local receiver on Windows or Apple Silicon.

**Run on Apple Silicon.** Download `Hand-Bridge-2.0.1-Apple-Silicon.zip`, unzip it
in Finder and double-click **Hand Bridge**. The ZIP includes Qt and the setup
guide. Live tracking uses Ultraleap Hyperion 6.2.0 for Apple Silicon, installed
separately. [Download the Mac release](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/releases/tag/v2.0.1)
· [Get Hyperion](https://www.ultraleap.com/downloads/leap-motion-controller-2/)

**Work on this version.** Download **Source code (zip)** from the same release
or clone the tagged source and create a working branch:

```sh
git clone --branch v2.0.1 https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions.git
cd Multiple-Leap-Motions
git switch -c my-hand-bridge-changes
```

The native source lives in `src/`, automated tests in `tests/`, and example
settings in `configs/`. On Apple Silicon, use the Xcode Command Line Tools,
CMake, Ninja, Python 3.9+, Qt 6.8.3 for macOS and the SDK from Hyperion for
real camera support. CMake retrieves the pinned open-source dependencies.
[Mac build, test and packaging commands](docs/reports/MACOS_ARM64.md#rebuild-the-local-preview)

Hand Bridge's own code is available under MIT. The Qt sources that match the
Mac app are attached to the release. Third-party components retain their
respective licenses. [MIT license](LICENSE) · [Mac dependency notices](packaging/macos/Third%20Party%20Notices.txt)

The new **VST-style controller is implemented**: rotary processing controls,
per-camera routing, all supported channel switches, OSC settings, profiles and
live diagnostics. It connects to a separate engine; closing or crashing the
window leaves tracking/OSC running.

The engine has an optional **real LeapC adapter** and an explicit simulator.
On Apple Silicon, the desktop build received two real streams and a local UDP
receiver decoded both cameras. Physical Windows acceptance and receipt inside
TouchDesigner remain open.
The original ImGui application/source/README remain available for rollback;
master and handbridge-v2 are unchanged. Read [current state](docs/CURRENT_STATE.md)
for exact verified builds and remaining work.

![Native Hand Bridge processing controls](docs/screenshots/processing.png)

## Use the Apple Silicon desktop app

For a fresh Mac or an upgrade, follow the **Start Here.txt** included with the
Apple Silicon ZIP. On upgrade, quit both the controller and the detached
`handbridge-engine` in Activity Monitor before replacing the app.
The controller title shows the installed version.
[Installation guide](packaging/MACOS_START_HERE.txt)

Open **Hand Bridge.app** in Applications. Click **Start engine** if the window
shows disconnected. On the Overview page, look for a fresh stream from each
device. If one camera stays at zero frames, leave the working camera attached
and reconnect the silent camera. Check Devices for the mapping between cameras and OSC aliases.

The default OSC destination is `127.0.0.1:9000`. Set an OSC In CHOP or OSC In
DAT in TouchDesigner to UDP port 9000. Move a hand over each camera to produce
palm and finger channels. Closing the controller leaves tracking running.
To stop it intentionally, quit `handbridge-engine` in Activity Monitor.
See the [local Mac build and hardware results](docs/reports/MACOS_ARM64.md).

## Run the native controller

The [Windows Release preview](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905/artifacts/14187119668)
includes both applications, Qt/plugins and the Microsoft runtime. Extract the
artifact, then the application ZIP inside it. Install Hyperion6.2 separately for
real tracking; **Start simulator** needs no cameras. See
[Release instructions](docs/WINDOWS_RELEASE.md) and [verified results](docs/reports/HYPERION6_RELEASE.md).

To build from source:

Install Visual Studio2022 Desktop development with C++, CMake3.24+, Python3.9+
(tests only), and official Qt6.8.3 MSVC2022 x64 desktop development files under
Qt's license. In PowerShell, adjust your Qt prefix:

```powershell
$env:Path = "C:\Qt\6.8.3\msvc2022_64\bin;$env:Path"
cmake --preset windows-control -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset windows-control --parallel 2
ctest --preset windows-control
.\out\build\windows-control\Debug\handbridge-control.exe --demo
```

Linux with the official Qt desktop package and OpenGL/EGL/xkbcommon development
files:

```sh
cmake --preset control-debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
cmake --build --preset control-debug --parallel 2
ctest --preset control-debug
./out/build/control-debug/handbridge-control --demo
```

`--demo` starts a detached simulated engine if needed. Start the controller again
to reconnect. To run the engine explicitly, launch the matching handbridge-engine
with `--simulate`; Ctrl-C shuts down a foreground host. A detached host can be
stopped explicitly using your OS process manager; closing the GUI intentionally
does not stop it. These Debug commands are for development. A separate Windows
Release ZIP includes both executables, Qt/plugins and the app-local Microsoft
runtime; see [Windows Release instructions](docs/WINDOWS_RELEASE.md). Ultraleap's
runtime remains an external installation; no SDK binaries belong in that ZIP.

## What the interface controls

- **Overview:** real camera freshness/FPS/counts, raw/processed measured-point
  previews, output rate/errors and engine connection.
- **Devices:** saved serial/alias identity, enabled output, left/right/both routing
  and independent source restart.
- **Processing:** raw bypass, time-based smoothing/deadzone, XYZ scale/rotation/
  offset, output units and optional pinch/grab hysteresis flags.
- **OSC Output:** enable, destination/port, send rate, all14 supported channel
  switches and socket reset. Legacy scalar OSC is the default/only protocol.
- **Profiles & Diagnostics:** apply/save status, atomic named profiles/startup
  restore with backups, bounded real logs and serial-redacted diagnostic export.

Edits are local until **Apply** receives an engine acknowledgement. **Applied**
and **Saved** are different; save a named profile to restore its applied snapshot
on restart. Current processing/channel controls are global, device routing is
per-camera. Per-channel processing overrides, full 3D/all-joint preview, bundles /
OSCv2 and an installer remain future work. The real source requires an SDK-enabled
build and a compatible installed x64 LeapC runtime.

Default output remains **127.0.0.1:9000**, one float32 per legacy address/datagram,
lowercase hands and raw millimeters. Smoothing/transforms are bypassed by default.
An explicit profile can change processed units; match TouchDesigner scaling.
OS-send success never proves receiver receipt. See [OSC protocol](docs/OSC_PROTOCOL.md)
and [TouchDesigner setup](docs/TOUCHDESIGNER_SETUP.md).

## Headless build without Qt or an SDK

The original SDK-free console build remains available:

```sh
cmake --preset core-debug
cmake --build --preset core-debug --parallel 2
ctest --preset core-debug
./out/build/core-debug/handbridge-engine --simulate --duration-ms 5000
```

Windows uses the corresponding windows-core preset. The plain console host reads
legacy JSON; the Qt IPC host additionally supports [schema2](docs/CONFIG_SCHEMA.md),
profiles and control commands. Working examples:
[legacy](configs/legacy-default.example.json),
[Qt studio default](configs/studio-default.example.json). The older proposed
handbridge.example.json is a design artifact, not the implemented schema.

## Continue with real cameras locally

Follow [LeapC setup](docs/LEAPC_SETUP.md) and the [Windows handoff](docs/WINDOWS_LOCAL_HANDOFF.md).
The SDK is absent from this repository. A development helper retrieves a
hash-verified SDK from the official **Hyperion 6.2.0 Windows installer** into
ignored work/, without running the installer. Its LeapC client is **6.2.0.0**.
It does not establish which SDK is installed on your PC.
Use your installed SDK's exact header when available. Build with
`HANDBRIDGE_WITH_LEAPC=ON` and `LEAP_SDK_ROOT`; the optional windows-legacy preset
still means the original GUI, independently of the new adapter.

The engine defaults to real tracking in an SDK-enabled build; `--simulate`
explicitly selects synthetic cameras. Check the runtime before connecting cameras:

```powershell
.\handbridge-engine.exe --check-leapc --leapc-library "C:\absolute\path\LeapC.dll"
```

This reports the loaded DLL path/version and verifies required API exports. It
does not open a service connection or prove camera/TouchDesigner operation. The
client and connected tracking service must each report **6.2.0 or newer 6.x**;
older or unknown versions are rejected. The GUI's **Start engine** action
performs the same asynchronous check before starting
the independent engine; **Start simulator** remains separate.

## Documentation

- [Apple Silicon desktop results and build instructions](docs/reports/MACOS_ARM64.md),
  [macOS port plan](docs/MACOS_PORT_PLAN.md) and [earlier hosted checks](docs/reports/MACOS_M0.md).
- [Specification](PROJECT_SPEC.md), [plan](PLANS.md), [agent instructions](AGENTS.md)
- [Current state](docs/CURRENT_STATE.md), [handoff](docs/SESSION_HANDOFF.md), [GUI build evidence](docs/reports/GUI_BUILD.md)
- [Architecture](docs/ARCHITECTURE.md), [local IPC](docs/CONTROL_PROTOCOL.md), [configuration](docs/CONFIG_SCHEMA.md), [UI](docs/UI_SPEC.md)
- [Code audit](docs/LEGACY_AUDIT.md), [testing](docs/TESTING.md), [reliability](docs/RELIABILITY.md), [hardware plan](docs/HARDWARE_TEST_PLAN.md)
- [Dependencies/licensing](docs/DEPENDENCIES.md), [decisions](docs/DECISIONS.md), [original README](docs/legacy/README.md)

Build into ignored out/, not the historically tracked build/ directory. Tooling,
local dependency downloads, screenshots-in-progress and scratch files go under work/.
