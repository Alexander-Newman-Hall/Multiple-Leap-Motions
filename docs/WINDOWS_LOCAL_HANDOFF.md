# Continue locally on Windows

The user subsequently authorized a full native GUI rebuild in cloud. It is now
implemented and mock/UDP tested. Continue its branch on the Windows PC for the
real SDK adapter; do not discard the controller/audit/test work.

## Get the work

Draft PR: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/2

```powershell
git clone --branch codex/handbridge-native-controller https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions.git
cd Multiple-Leap-Motions
```

If already cloned, preserve local edits, fetch origin and switch to the development
branch safely. Do not reset, delete or force-push master/handbridge-v2.
Open this local checkout in Codex on the Windows PC. A new cloud task cannot
access your USB cameras just because it points to the same GitHub repository.

## Machine prerequisites

- Windows 10/11 x64, VS2022 Desktop development with C++ and Windows SDK.
- Git, CMake3.24+, Python3.9+ (test receiver only).
- Official compatible Ultraleap Hyperion runtime and LeapC development SDK;
  record exact installed versions and obey its license. No SDK files are vendored.
- Two controllers, USB connections and TouchDesigner for physical validation.

First establish the SDK-free baseline locally:

```powershell
cmake --preset windows-core
cmake --build --preset windows-core --parallel 2
ctest --preset windows-core
```

The new handbridge-engine is **simulation only**. It does not yet have a LeapC
adapter. The optional windows-legacy preset builds the old GUI reference and is
unverified; it is not a real-device mode of the new host. Set LEAP_SDK_ROOT to the
installed SDK directory containing include/LeapC.h and lib/x64/LeapC.lib when
working on SDK integration. Do not copy SDK DLLs into git.

## Paste into the next local Codex task

> Read AGENTS.md, PROJECT_SPEC.md, PLANS.md, docs/CURRENT_STATE.md,
> docs/SESSION_HANDOFF.md, docs/reports/*_AUDIT.md and relevant protocol/hardware
> documents. Continue the existing feature branch; preserve original branches.
> Implement only P2.1: the smallest real LeapC ITrackingSource adapter and Windows
> x64 build seam for the tested headless runtime. Inspect exact installed SDK
> headers and official docs before using APIs; record SDK/service versions.
> Use RAII, explicit multi-device subscriptions, one poll owner, bounded timeout,
> owned frames and serial-based identity. Keep the simulated/OSC/config tests
> passing. Then verify two real devices and actual TouchDesigner receipt with
> the user's physical interaction, recording exact results or blockers. Preserve
> the existing Qt controller/IPC/processing; do not add fusion or packaging. Update current state,
> hardware results and handoff with one next task. A compile or simulator pass
> must never be reported as a physical camera test.

Human hand movement, unplug/replug and receiver observations must actually be
performed on that PC. Follow HARDWARE_TEST_PLAN.md; label unperformed cases
UNTESTED ON HARDWARE. The field silent-stall root cause remains unproven.

## Build the new native controller first

Install official Qt6.8.3 MSVC2022 x64 desktop development files; use root README's
windows-control commands with CMAKE_PREFIX_PATH and Qt bin on PATH. The controller
already implements modern VST-style settings and survives engine GUI detachment.
Use handbridge-control.exe --demo to see it with mock cameras; this is not a real
LeapC mode. Keep native UI/process and55 Qt-free tests passing when adding SDK
support. Actual hardware still needs this local PC and human observations.
