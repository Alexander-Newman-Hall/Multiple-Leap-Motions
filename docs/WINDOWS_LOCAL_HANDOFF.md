# Continue locally on Windows — Hyperion 6.2 acceptance

The new controller and real LeapC adapter are implemented. Continue the feature
branch; do not repeat the GUI rebuild or discard the prior audit/test work.
Actual cloud/Windows Release results and exact tested commits are in CURRENT_STATE
and SESSION_HANDOFF. A DLL load or mock stream is not a physical camera test.

## Get the work

```powershell
git clone --branch codex/handbridge-leapc-release https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions.git
cd Multiple-Leap-Motions
```

Preserve local edits when switching an existing checkout. Master, handbridge-v2
and prior feature branches remain available; never reset/delete/force-push them.
Open this checkout in local Codex. Cloud Codex cannot access these USB cameras.

For a prebuilt developer preview, follow WINDOWS_RELEASE.md and choose a
successful workflow artifact. Extract the full ZIP. It includes Qt and release
CRT; Visual Studio/Qt/Python are build tools, not requirements for running that
package. The official Hyperion runtime/SDK remains an external prerequisite.

## Establish the correct runtime before cameras

Install current official **Hyperion 6.2.0** compatible with your devices and record
the exact Windows/SDK/client/service versions. The required API policy is6.2.0 or
newer 6.x for both client and connected tracking service. Old/unknown versions fail
clearly before healthy state. Use the actual installed x64 client DLL:

```powershell
.\handbridge-engine.exe --check-leapc --leapc-library "C:\Program Files\Ultraleap\LeapSDK\lib\x64\LeapC.dll"
.\handbridge-engine.exe --headless --leapc-library "C:\Program Files\Ultraleap\LeapSDK\lib\x64\LeapC.dll"
.\handbridge-control.exe
```

The GUI can perform the same check with Start engine. For a nonstandard DLL path,
set HANDBRIDGE_LEAPC_LIBRARY before opening the controller. Start simulator is
explicitly separate. Stop an already-running detached simulator in Task Manager
before switching to a real engine on the same endpoint. Closing the GUI does not
stop either engine. Full path policy and developer builds: LEAPC_SETUP.md.

The preflight reports the loaded DLL file/API versions with no service connection
or camera. Once connected, the engine separately checks the actual ServerLibrary
version. Record observations; do not copy SDK binaries into Git.

## Paste into the next local Codex task

> Read AGENTS.md, PROJECT_SPEC.md, PLANS.md, CURRENT_STATE, SESSION_HANDOFF,
> LEAPC_SETUP, WINDOWS_RELEASE and HARDWARE_TEST_PLAN. Continue
> codex/handbridge-leapc-release. The GUI, IPC and genuine Hyperion 6.2 adapter
> already exist; preserve them. First query the installed client/service versions
> and establish the cloud-test baseline locally. Then execute only the physical
> two-camera acceptance: distinct serial/aliases and simultaneous output, no hands,
> real TouchDesigner OSC receipt, unplug/replug, controlled tracking-service
> restart, per-device filtering/restart, GUI close/kill/reopen and a measured soak.
> Record actual PASS/FAIL/BLOCKED and exact versions in HARDWARE_TEST_RESULTS.md.
> Diagnose observed failures from source/subscription/frame/output metrics and
> fix only a bounded demonstrated issue; do not add fusion or redesign the UI.
> Update current state and handoff with the exact tested commit and one next task.

Human hand motion, unplugging and receiver observations must actually happen on
that PC. Mark unperformed hardware cases NOT RUN. The original field silent-stall
incident remains unproven until reproduced/measured; mock recovery tests do not
establish its real root cause.
