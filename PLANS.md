# Executable implementation plan — Hand Bridge 2.0

This is a **phased plan for multiple Codex tasks**, not a request to finish everything in one task. Each task should produce a reviewable PR with precise acceptance evidence and updated handoff docs. Follow `PROJECT_SPEC.md` for product requirements.

## Execution checkpoint — 2026-11-15

P0 inventory/build feasibility and source-backed OSC/config baseline are delivered
in this branch; see docs/CURRENT_STATE.md for exact tests and remaining limits.
The bounded simulation foundation and native controller/settings slice are
delivered. A real LeapC adapter and Windows portable packaging are the current
user-authorized increment. File replay, full stage metrics, automatic output
reset, OSCv2 and physical acceptance remain separate work. Acceptance evidence
belongs in the living state/handoff, not inferred from these checkboxes.

## User-approved controller rebuild — delivered

The user explicitly requested a complete modern native GUI rebuild with a VST
feel and working settings, after the initial pause for local hardware work. This
instruction authorizes Phase 3 and the minimum Phase 4 settings/processing needed
by its controls using the tested mock source now. Phase 2 remains BLOCKED ON
INSTALLED SDK/HARDWARE; the GUI must clearly label simulated tracking. Original
branches/UI stay recoverable. This changes task order, not hardware acceptance.

Delivered: portable settings `2de385f`, native IPC/profile host `99622f2`,
VST-style Qt controller `9f5260e`, applied-preview correction `ebb3bd9`; final green
Windows/Ubuntu native CI at `ebb3bd9`, including repeated Windows lifecycle checks.
Actual results and remaining hardware gate are in CURRENT_STATE/SESSION_HANDOFF.
Preserve this controller while adding the real source and deployment checks.

Bounded increments: (1) validated engine settings and real processing effects;
(2) same-user native IPC, atomic profiles, independent engine lifecycle;
(3) Qt Quick controller with five functional pages and VST-style controls;
(4) packet/process/UI lifecycle tests, screenshots, Windows/Linux build evidence
and handoff. No fusion, web frontend, real-camera claim or SDK API invention.

## User-approved SDK and release increment — active task

The user explicitly requested the real adapter and a Windows Release ZIP before
connecting cameras, and clarified that **latest Hyperion v6** is required. Use the
SDK files from the current official Hyperion6.2 Windows package, not the earlier
Unreal-plugin5.30 development baseline. Record the actual header/client/service
versions separately. This authorizes P2.1 plus the portable portion of P5.2;
it does not waive the P2/P5 physical acceptance gates.

Deliver a real-header compiled adapter, deterministic injected-API tests, runtime
DLL preflight, SDK-independent fallback, separate real/simulator GUI actions,
Release deployment with Qt/MSVC runtimes and a PATH-clean extracted-ZIP smoke.
Keep SDK licence boundaries explicit and preserve all earlier branches.

## P0: Baseline and evidence (start here)

**Task P0.1 — Repo inventory:** Pin the starting commit; map CMake + VS sources/dependencies, launch path, device callbacks, processing, OSC, configuration and UI links. Identify duplicate/dead modules and tracked generated artifacts. Deliver `docs/LEGACY_AUDIT.md` with file/line references and test status.

**Task P0.2 — Baseline build/test:** Make clean SDK-independent build possible, or document exact missing dependencies and produce smallest safe mock target. Do not claim full hardware binary. Resolve C++17→C++20 build migration carefully. Deliver real CMake commands, pinned toolchain and CI feasibility.

**Task P0.3 — OSC/config golden contract:** Generate a test fixture reproducing current addresses, lowercase handedness, units, default port and loss behavior. Write alias/config regression test (including `deviceAliases` vs `device_aliases`). Deliver decoded OSC packet fixture and migration plan.

*Phase 0 gate:* Source/behavior baseline is independently reproducible or limitations are precisely documented. No destructive refactor before this gate.

## P1: Headless engine foundation

**Task P1.1 — Core value types:** Introduce device/frame/hand IDs, source+arrival timestamps and validity with minimal SDK coupling. Preserve legacy data conversion.

**Task P1.2 — Mock/replayer:** Deterministic multi-camera `ITrackingSource` with injected timestamps, disconnect/service events and empty-frame scenarios; run with no LeapC hardware.

**Task P1.3 — Engine/runtime split:** Extract runtime from `MainAppWindow`/SDL event loop. Runnable `handbridge-engine` mock target produces OSC without GUI. Ensure clean shutdown and lifetime ownership.

**Task P1.4 — Reliability instrumentation:** Stage counters, per-device liveness, drop metrics and structured logs. Fault-injection tests for stalled poller, queue overflow and missing GUI.

**Task P1.5 — Legacy OSC transport hardening:** Preserve legacy output; eliminate unnecessary hot-path logs; optional bounded frame bundles (opt-in); sender status/metrics and graceful socket failure behavior.

*Phase 1 gate:* Two simulated camera streams → localhost OSC without UI; exact v1 contract tested; injected failures identified and bounded.

## P2: Windows LeapC adapter and physical recovery

**Task P2.1:** SDK-aware adapter compiled against installed Hyperion version on Windows; RAII handles/subscriptions and per-device events.

**Task P2.2:** Service loss/reconnect, device unplug/replug, stale/invalid event transitions, stable alias map. Unit/mock tests first.

**Task P2.3:** Windows hardware test with two cameras and TouchDesigner; capture root cause evidence for any stream stoppage; record errors, counters and recovery.

*Phase 2 gate:* Hardware tests in `docs/HARDWARE_TEST_RESULTS.md`, or explicitly BLOCKED and not declared complete.

## P3: Optional native GUI / IPC

**Task P3.1:** Versioned, local-only IPC with `get_status`, `get_config`, `patch_config`+ack, telemetry subscriptions; bound message size, same-user access, single instance.

**Task P3.2:** Qt 6 overview/devices/OSC screens with real engine models and output status. Verify disconnect/reconnect of GUI.

**Task P3.3:** Processing editor, profiles/diagnostics, preview and accessibility. Every displayed interactive control connects to live engine state; no fake controls.

*Phase 3 gate:* UI close/kill/reopen does not interrupt oscillator output; all visible controls are backed by acknowledged engine changes.

## P4: Processing and calibration

**Task P4.1:** Time-based smoothing/deadzone, scale/offset, correct unit conversion, raw bypass. Test channel-independent behavior and loss reset.

**Task P4.2:** Optional per-device world transform for fixed 3D-printed rig, with known coordinate conventions; no fusion or handoff.

**Task P4.3:** Stable versioned configs/presets, migration, backup, atomic save and UI apply state.

*Phase 4 gate:* Deterministic raw→processed→OSC tests; save/restart equals previous output.

## P5: Release hardening

**Task P5.1:** Long-run soak testing, repeated device/service failure recovery and profiling in Windows.

**Task P5.2:** Installer or portable ZIP, legal third-party notices, production logging defaults, startup-at-login option if needed.

**Task P5.3:** TouchDesigner example network/protocol cheat sheet, full user README/operations runbook and release notes.

*Phase 5 gate:* Physical tests, real OSC receipt, performance/limitations and traceable release artifacts; no unverified “production ready” assertion.

## Cross-cutting done criteria on EACH task

- Source changed and reviewed in a small PR.
- Unit/CI tests actually run (or exactly documented blocked).
- No silent OSC/config behavior change.
- One short updated ADR when architecture changes.
- `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`, and issue list are current.
- One recommended next step, with success criterion, supplied to the next Codex run.
