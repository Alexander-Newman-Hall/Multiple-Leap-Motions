# Executable implementation plan — Hand Bridge 2.0

This is a **phased plan for multiple Codex tasks**, not a request to finish everything in one task. Each task should produce a reviewable PR with precise acceptance evidence and updated handoff docs. Follow `PROJECT_SPEC.md` for product requirements.

## Execution checkpoint — 2026-11-15

P0 inventory/build feasibility and source-backed OSC/config baseline are delivered
in this branch; see docs/CURRENT_STATE.md for exact tests and remaining limits.
P1.1–P1.5 begin as one bounded simulation vertical slice: owned event envelope,
mock source, independent console host, mailbox/freshness/retry counters and checked
scalar sender. File replay, full stage/FPS metrics, automatic output reset, real
LeapC adapter and Qt/IPC are not implemented. This is not completion of all P1
capabilities or the hardware gate. Acceptance evidence belongs in the living
state/handoff, not inferred from these checkboxes.

The next task is a narrow P2.1 installed-SDK adapter compile/ownership seam plus
Windows evidence; retain the tested mock contract and block physical claims
until hardware results exist. Do not skip ahead to GUI or processing features.

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
