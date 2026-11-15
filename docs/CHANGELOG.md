# Hand Bridge 2.0 changelog

This file tracks **implemented** changes to the application. Do not list planned features as shipped. Keep dates/versions and commits with each entry.

## Unreleased — Phase 0 audit / Phase 1 simulation, 2026-11-15

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
