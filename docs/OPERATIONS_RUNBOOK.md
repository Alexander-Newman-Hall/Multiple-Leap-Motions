# Running the native controller and engine

Build/package instructions: root README, [WINDOWS_RELEASE](WINDOWS_RELEASE.md),
[LEAPC_SETUP](LEAPC_SETUP.md) and [GUI_BUILD](reports/GUI_BUILD.md). The controller
and engine remain separate ordinary user processes; no elevated service or hidden
tracking-service restart is installed.

## Real tracking on Windows

Install the official Ultraleap Hyperion runtime/development files and tracking
service separately. Hand Bridge does **not** bundle the proprietary LeapC SDK DLL.
Client and service must both report **6.2.0 or newer within 6.x** through
`LeapGetVersion`; an older client/service or a different major version is rejected.
A DLL file version such as `6.2.0.0` identifies the installed artifact but does not
replace the client/service API-version checks.

In a build with `HANDBRIDGE_WITH_LEAPC=ON`, real LeapC is the default:

```powershell
.\handbridge-engine.exe --check-leapc
.\handbridge-engine.exe --headless
.\handbridge-control.exe
```

Use the executable's built/installed path. The controller's **Start engine** button
runs an asynchronous `--check-leapc` preflight and surfaces missing/unsupported
runtime errors. On success it starts the real engine detached. The preflight
child is separate from the tracking process; neither rendering nor controller
lifetime owns tracking. A successful DLL check is not a physical tracking test.

The loader selects an absolute library path in this order:

1. Explicit `--leapc-library` path.
2. `HANDBRIDGE_LEAPC_LIBRARY` environment variable.
3. `LEAP_SDK_ROOT/lib/x64/LeapC.dll` when that SDK root is configured.
4. The installed `Program Files/Ultraleap/LeapSDK/lib/x64/LeapC.dll` location.

It does not search the working directory or arbitrary PATH entries. An explicit
or configured path must be absolute and point to the user's compatible installed
x64 runtime. For example, from PowerShell:

```powershell
$env:HANDBRIDGE_LEAPC_LIBRARY = 'C:\path\to\installed\LeapC.dll'
.\handbridge-engine.exe --check-leapc
```

The GUI inherits the environment used to launch it; its preflight and detached
engine use the same default/environment selection. The headless host can also
use `--leapc-library` directly. SDK-off builds report the unavailable provider
instead of generating simulated hands. Non-Windows runtime loading is unsupported;
header/API and mock tests on those systems do not establish hardware support.

## Simulation and detached operation

Simulation is explicit, with no cameras required:

```text
handbridge-control --demo
handbridge-engine --simulate --headless --scenario stall
```

The controller's **Start simulator** button has the same explicit mock meaning.
`--demo` leaves an already connected engine alone. The stall scenario requires
simulation; `--simulate` cannot be combined with `--leapc-library` or
`--check-leapc`. OSC destination/settings still affect real emitted UDP in mock
mode. Look for the SIMULATED versus LEAPC SOURCE badge and `source`/`simulated`
telemetry; `hardware_verified` remains false in either mode.

Both processes use the default per-user local endpoint unless `--endpoint` is
supplied. Closing/crashing the window intentionally leaves the detached engine
alive; reopen the controller to reconnect. Stop a foreground host with Ctrl-C or
stop a background host explicitly in the OS process manager. Starting a second
engine on the same endpoint fails without replacing the active engine.

`--duration-ms N` limits a headless run; absent/zero duration runs until stopped.
`--help` and `--version` describe the selected build. Exit 0 from preflight means
only client DLL/export/API validation. Exit 0 from a bounded engine run means
clean completion, not proof of cameras or OSC receipt; inspect stage state.
Startup/argument failures exit nonzero and observed UDP send errors are reported
separately. The actual receiver must verify TouchDesigner receipt.

## Applied settings, profiles and diagnostics

Edit settings then Apply; only engine acknowledgement is Applied. Save a named
profile to write the applied snapshot/current startup state atomically with a
backup. Dirty local editors cannot be saved/loaded silently. Rejected settings
leave the last valid engine state. A profile load can apply then encounter a
persistence error; read its explicit applied-but-unsaved response.

The state/profile directory is private per-user HandBridge data, overridable by
`--state-dir` on the Qt host. Explicit `--config` accepts legacy or implemented
schema 2 there. Old flat config is imported without overwriting it. The Qt-free
host still accepts legacy flat configuration only. Provider selection is a launch
choice; loading a profile does not silently switch real/mock tracking.

Device restart and output-socket reset are explicit UI commands. The service,
device presence, subscription, fresh frames, visible hands and OSC send state
remain distinct. Normal frames with zero hands are healthy. Incompatible service
API versions are not reported as healthy service connections.

Provider diagnostics show actual selected library path/file version, the service
API version when obtained, retained last error, connection attempts and polling
errors. `--check-leapc` additionally prints `client_api_version`, independently of
its `version` file-resource field. Do not treat a retained historical error as a
current failure without checking current service/frame state. Ordinary no-frame
poll timeouts are not polling errors. Send counters describe OS acceptance, never
TouchDesigner acknowledgement.

**Export diagnostics** writes bounded state/config/logs into the private engine
diagnostics folder and reports its actual path. Known serials and the library-path
field are redacted in the exported file; the local live display and explicit
preflight output can show the actual installed path for troubleshooting. See
[CONTROL_PROTOCOL](CONTROL_PROTOCOL.md) for exact fields and command semantics.

## When output stops

1. Inspect service, device, subscription, frame age and processing/send metrics.
   If frames continue with no hands, do not restart working hardware.
2. In real mode, check the retained source error, `service_version` and client
   preflight. Both sides need 6.2+ within 6.x; an updated DLL with an old service
   is not a compatible installation. Capture the exact versions/errors.
3. For one stale camera, capture diagnostics and use its targeted restart; confirm
   the other camera keeps updating. Do not assume USB reconnection from a mock test.
4. Check destination, enabled channels and the actual TouchDesigner OSC In DAT/CHOP
   listener. OS send success alone does not establish receipt.
5. If the GUI cannot attach after Start engine, run the headless command in a
   console to inspect startup/configuration errors. Keep existing config backups.

The adapter and version checks do not establish physical acceptance. Real hand
motion, two cameras, USB unplug/replug, service recovery, TouchDesigner receipt
and soak remain **UNTESTED ON HARDWARE** until documented on the local Windows PC.
Follow [HARDWARE_TEST_PLAN](HARDWARE_TEST_PLAN.md); write actual results or BLOCKED
rather than inferring success from a compile, fake API, preflight or simulator.
