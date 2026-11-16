# Current state — native Apple Silicon arm64

Branch: `codex/handbridge-macos-feasibility`; starting source 48f2e17.
**Mac product scope is native Apple Silicon arm64 only.** No Intel build, Rosetta
or universal release requirement. Historical Intel results do not pass arm64
acceptance. Windows x64, C++20, Hyperion 6.2, Qt GUI and legacy OSC/config remain.

Darwin sender now sets SO_NOSIGPIPE per socket before connect/send, checking errors
through existing RAII/counters. Global signal policy is untouched. Existing
broadcast regression remains; new Darwin child test defaults/unblocks SIGPIPE and
checks error counting plus unchanged disposition. ADR-018 records the decision.

Changed paths: OscSender, legacy OSC tests, arm64-only presets/Mac workflows,
SDK fetch/diagnostic architecture guards, native GUI smoke, plan/spec/README,
OSC_PROTOCOL/DECISIONS and this state/handoff. [Arm64 report](reports/MACOS_ARM64.md)
separates new results from [historical M0](reports/MACOS_M0.md) and
[historical concurrency evidence](reports/MACOS_M2_CONCURRENCY.md).

| Acceptance | Current result |
|---|---|
| Linux local regression | Build, legacy OSC **8/8 PASS** (0.05 s), full native suite **64/64 PASS** (22.49 s); not Darwin/arm64 evidence |
| Native arm64 engine / unchanged GUI / genuine 6.2 SDK | CI prepared; actual arm64 results pending; no Intel substitution |
| Local Apple Silicon access | Not available; attached machine is Linux x86_64 |
| Physical Mac service / two cameras / recovery / TD | **NOT RUN** |

The arm64 job builds/tests engine and GUI, checks application slices, loads the
pinned genuine arm64 SDK in the owned diagnostic, and exercises a separate native
Cocoa GUI/engine/UDP lifecycle smoke. SDK-header/injected tests are software tests;
production Mac loader integration remains M3. No test is skipped to hide SIGPIPE.
If hosted capacity fails, report exact blocker and leave arm64 checks unrun.

**Milestones:** arm64 M0/M2 acceptance pending; protected Mac IPC remains open;
M1 physical NOT RUN; M3 real integration and M4/M5 packaging/hardware not delivered.
**Next exact task:** collect arm64 CI evidence (or its concrete capacity blocker),
record compiler/platform/SDK and GUI outcomes, then hand off remaining gates.

## Preserved Windows and planning history

# Current state — Windows preview and macOS planning

Current planning branch: `codex/handbridge-macos-plan`, based on Windows handoff
`2c1ce8e72980c0ef4d59efa474e19df30c2666a2`.
Preserved Windows release branch: `codex/handbridge-leapc-release`.
Draft [PR #3](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/3)
is stacked on the preserved native-controller branch.
Validated application/packager source: **`c8596f20b35bb0e4d09446933c543f689417f36c`**.
The final handoff follows with documentation/screenshots only; application code is
identical to the tested source. Original master `054e578`, handbridge-v2 5f54718,
controller `3825ecd` and foundation `30cd5e3` remain preserved.

## Delivered

- Real native LeapC adapter compiled against the SDK from the official **Hyperion
  6.2.0 Windows installer**, with client DLL **6.2.0.0-c98d293a**.
- One poll owner, multi-device-aware connection, explicit subscriptions, stable
  serial identity, owned raw observations/native hand IDs and independent recovery.
  Bounded polling, handshake timeout, capped retries and orderly handle cleanup.
- Actual SDK version policy: both client API and connected ServerLibrary must
  report **6.2.0 or newer within 6.x**. Older/unknown/other-major versions never
  become healthy service state. No silent fallback to simulated hands.
- Safe absolute DLL selection, --check-leapc preflight, real DLL/service diagnostics,
  GUI Start engine versus Start simulator and asynchronous startup errors. The
  optional GUI still does not own tracking/processing/OSC lifetime.
- Known native hand-ID changes reset only that hand's smoothing/deadzone/gestures.
  Legacy OSC/default 127.0.0.1:9000/raw millimeters/schema 2 remain unchanged.
- **Windows x64 Release ZIP:** both executables, Qt/QML/plugins, app-local Microsoft
  Release CRT, notices and corresponding Qt source. SDK/client/service are external.
  No proprietary SDK file or debug runtime is included.

[Download the final Windows preview](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905/artifacts/14187119668) (approximately 108 MB).
[Successful Release run](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905). Extract the artifact, then the application ZIP
inside it. Instructions: WINDOWS_RELEASE.md and LEAPC_SETUP.md.

## Verified evidence

| Scope | Actual result |
|---|---|
| Cloud GCC 14.2 / Qt 6.8.3 / genuine 6.2 header |Configure/build PASS; **82/82**, no skips/failures,20.48 s |
| SDK-type adapter lifecycle/version cases |**21 passed**, production adapter + injected SDK calls |
| ASan/UBSan with leak checking |**34/34**, zero diagnostics; project adapter/process/runtime; no Qt/SDK-binary instrumentation |
| SDK download/extraction helper |**7/7** offline regressions; real verified installer extraction passed Linux and Windows |
| Windows Server 2022 / MSVC 19.44 / genuine 6.2 header |**81 passed +1 expected POSIX-only skip**, zero failures,32.03 s |
| Windows process-lifecycle repeat gate |Three until-fail runs passed 19.09/19.08/19.07 s; no retry-until-success |
| Actual Windows LeapC client |DLL 6.2.0.0 / ClientLibrary API 6.2.0, all 12 exports PASS |
| Actual SDK without service/cameras |3600 ms bounded run,2 attempts, truthful no-service/no-device/no-OSC, clean exit |
| Extracted ZIP / Windows native GUI |**16 checks passed**: clean PATH, app-local Qt/CRT, actual rendering, OSC across GUI close/kill/reopen, invalid DLLs and Unicode path |
| SDK-free core + native-controller CI |Windows/Ubuntu both PASS at the same source commit |
| Physical cameras/USB/Hyperion service/TouchDesigner |**UNTESTED ON HARDWARE** |

Package manifest records a **clean** source checkout and the exact 6.2 header hash:
`27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765`.
The published ZIP/checksum/manifest were independently downloaded and inspected.
[Detailed report](reports/HYPERION6_RELEASE.md) includes commands, CI links, SDK
provenance, scope limits and the two corrected setup/provenance defects. Actual
Windows captures: [Overview](screenshots/windows-overview.png),
[Processing](screenshots/windows-processing.png).

## Limits / single next task

No physical two-camera stream, service restart, USB hotplug, TouchDesigner receipt
or hardware soak is inferred. Cleaned Windows CI paths are not a fresh consumer
Windows 10/11 installation. The owner’s installed versions are still unknown;
preflight and the service gate report/check them locally.

Processing/channel controls remain global, identity/routing per-device. No fusion,
OSC bundles/v2, per-channel inheritance, full 3 D/all-joint preview, installer or
automatic startup. Legacy optional orientation/velocity/normal loss quirks remain
in OSC_PROTOCOL. The original field silent-stall root cause remains unproven.

## macOS planning checkpoint — 2026-11-16

Added [MACOS_PORT_PLAN](MACOS_PORT_PLAN.md): six bounded milestones, source-level
port inventory, official support evidence, compiler/IPC risks, acceptance gates
and a copyable next-chat prompt. This task changes documents only; no Mac build,
SDK load, hardware check or package was performed. Documentation links and source
change boundaries are checked; prior test counts above are historical verified
Windows/Linux results, not new tests from this planning task.

**Single next Mac task:** run M0 in local Mac Codex: inspect the actual Mac SDK /
toolchain, compile/link feature probes and build a minimal bounded SDK diagnostic.
Two-camera support remains unverified until M1 physical observations. Keep Windows
physical acceptance open separately, using WINDOWS_LOCAL_HANDOFF and
HARDWARE_TEST_RESULTS; do not replace either platform's missing evidence with mocks.
