# Hand Bridge 2.0 changelog

## 2.0.1 — Apple Silicon installation update, 2026-11-18

- Give each camera three seconds to deliver its first frame after subscribing.
  Once streaming starts, stop using a pose after 500 ms without a fresh frame.
- Preserve the increasing retry delay when the SDK refuses a subscription.
  Reconnecting a camera starts a new retry schedule for that camera.
- Ship an operator ZIP that verifies the extracted application's code signature,
  includes the upgrade instructions and declares macOS 12 as the minimum version.

## Unreleased — native Apple Silicon scope and Darwin SIGPIPE, 2026-11-16

- Narrowed planned Mac product to native arm64; removed Intel presets/runner
  fallback and Rosetta/universal-release requirements. Intel history is retained
  but is not arm64 acceptance.
- Set Darwin SO_NOSIGPIPE per OSC socket before connect/send with checked failure
  and cleanup; preserved global signal policy and Windows/Linux/legacy OSC paths.
- Added default/unblocked SIGPIPE regression and arm64 engine/GUI/SDK checks,
  including native Cocoa lifecycle evidence. Actual native execution/capacity is
  recorded in reports/MACOS_ARM64.md. Both arm64 pools failed acquisition before
  any step; validation is BLOCKED, and no unexecuted check is declared passed.

## Unreleased — M0 evidence and narrow M2 concurrency slice, 2026-11-16

- Verified final owned SDK diagnostic on Intel with genuine Mac Hyperion 6.2.0:
  missing library, real client loading and bounded no-service cleanup. Arm64
  allocation failure remains infrastructure evidence; physical M1 NOT RUN.
- Replaced only the host's unavailable jthread/stop-token and atomic shared_ptr
  primitives with an owned thread, explicit stop/wake/join and immutable mutex
  snapshots (ADR-017), retaining C++20, Hyperion 6.2, GUI and legacy OSC/config.
- Added focused snapshot/worker regressions and SDK-free Intel Mac host CI. Local
  64/64 native tests and Windows/Linux CI pass. Both Mac hosts compile/link, and new worker/snapshot tests pass. Full Mac suites
  retain the separate existing broadcast test SIGPIPE (59/60 core, 60/61 host);
  full M2 is not passed or distributable.
- Exact commits, native results and remaining blockers are in the M0/M2 reports;
  no real tracking service, two-camera recovery or TouchDesigner acceptance claim.

## macOS port plan — 2026-11-16

- Added six staged milestones and a bounded local-Mac first-task prompt; preserved
  the Windows release and identified two-device support as unverified.
- Included code-level loader, Apple C++ library, IPC permission, deployment and
  architecture gates, with cloud versus physical evidence requirements.
- Updated plan/spec links, current state and handoff; corrected an installer-hash
  formatting typo in the earlier Windows notes.
- Documentation only: no application changes, SDK downloads, Mac builds or camera
  tests. Documentation link/source-boundary checks are the task validation.

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
