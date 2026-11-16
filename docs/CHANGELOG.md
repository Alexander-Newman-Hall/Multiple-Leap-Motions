# Hand Bridge 2.0 changelog

## Hyperion 6.2 adapter / portable preview — 2026-11-15

- Added a native real LeapC source using the exact official Hyperion 6.2 header,
  owned frame copies, native hand IDs, explicit multi-device subscriptions and
  bounded independent recovery; both headless hosts can select it.
- Required client/server versions6.2.0 or newer 6.x with actual SDK version calls;
  added safe DLL loading, nontracking preflight and real GUI startup/diagnostics.
- Reset positional/gesture state when a known native hand identity changes.
- Added21 genuine-SDK-type injected lifecycle/version tests and7 SDK-fetch
  regressions; full Linux native suite82/82 passed. Windows evidence follows in
  CURRENT_STATE rather than being inferred from these tests.
- Added optional Windows Release portable deployment and extracted-package
  verification, Qt/QML/plugins, app-local CRT, notices/corresponding Qt source.
- Preserved legacy OSC/default port/units/config schema and original branches.
  Physical camera/Hyperion recovery/TouchDesigner receipt remain unverified.

This file tracks **implemented** changes to the application. Do not list planned features as shipped. Keep dates/versions and commits with each entry.

## Unreleased — native controller rebuild, 2026-11-15

- Added a separate native Qt6 Quick/QML VST-style controller with5 functional
  pages, editable rotary controls, engine-backed device/channel/processing/OSC
  settings, acknowledged Apply/Saved states, profiles and diagnostics.
- Added strict schema2/legacy import, actual time-based filters/transforms/units,
  optional pinch/grab hysteresis channels and per-device routing/mute/restart.
- Added bounded same-user local IPC and an independent engine worker; GUI close /
  kill/reopen does not stop observed mock UDP output. Engine QtCore/Network only.
- Added atomic named/current profiles/backups, revision conflict and delayed-save
  safeguards, measured telemetry and redacted exports.
- Native cloud58/58 tests pass; targeted sanitizer56/56 pass. Actual native PNGs
  reviewed; Windows CI and physical boundaries recorded in CURRENT_STATE.
- Preserved original sources/branches, default raw-mm scalar OSC/9000 and old
  goldens. New source remains simulation-only; no hardware/release claim.

## Phase 0 audit / Phase 1 simulation, 2026-11-15

- Added code-level inventory, baseline failure evidence, original README/CMake
  copies, pinned C++20 SDK-free CMake presets and Windows/Linux CI.
- Fixed undefined first-run config settings, alias persistence, transactional
  legacy validation, malformed ports/assignments and write-error reporting.
- Preserved scalar raw-mm OSC, default 9000 and all existing loss quirks; added
  full decoded UDP goldens, actual native send metrics and bounded logging.
- Added independently runnable simulated headless engine, source seam, per-device
  latest mailbox/liveness/loss invalidation and capped recovery tests.
- No actual SDK adapter, Qt controller/IPC, v2 output, bundling, replay-file CLI,
  advanced processing/presets or physical reliability claim is included.
- Tests/evidence and exact commit are in CURRENT_STATE / SESSION_HANDOFF.

## Specification kit (historical)

- Prepared a proposed C++20 native modernization specification.
- Defined backwards-compatible OSC design, headless engine separation, optional native Qt controller and hybrid workflow.
- Defined error recovery/diagnostics and Windows physical test acceptance gates.
- No application source changes, releases, builds or hardware tests are claimed in this package.

## Template for future entries

### Version / date / commit
- Added:
- Changed:
- Fixed:
- Removed (with migration):
- OSC or configuration compatibility impact:
- Tests performed / limitations:
