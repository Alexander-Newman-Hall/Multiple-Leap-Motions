# Session handoff — Phase 0 audit checkpoint

Date: 2026-11-15. Environment: Linux cloud, no LeapC SDK, Windows or devices.
Branch: `codex/handbridge-phase0-reliability`.
Base: `handbridge-v2` at `5f547181f6d512ab6a8d2146022227e26cec48d2`.
Original `master`: `054e578525a5d5202ea8f6eb417ce55ded7d61b8` (rechecked).
This audit checkpoint is identified by its commit subject
`docs: record Phase 0 code audit and preserve original README`; implementation
checkpoint follows as a separate commit. Neither original branch is modified.

## Audit changes

- `docs/LEGACY_AUDIT.md`: actual executable path, preservation/repair/refactor/
  replacement decisions, prioritized source defects versus unproven hypotheses.
- `docs/reports/TRACKING_AUDIT.md`: full tracking/UI/thread and SDK evidence.
- `docs/legacy/README.md`: verbatim original README.
- `docs/CURRENT_STATE.md`, this file: truthful phase checkpoint.

Evidence commands: `git ls-remote --symref origin HEAD refs/heads/master
refs/heads/handbridge-v2`, `git diff origin/master..origin/handbridge-v2 -- src
tests CMakeLists.txt leapMotionApp.vcxproj`, source reads with `nl -ba`, official
Ultraleap HTTPS documents (links/checksums in tracking report). Audited source
matches master. No build or physical PASS is asserted by this audit commit.

Implementation underway: baseline C++20 SDK-free build, golden OSC/config tests,
minimal headless runtime with simulator and isolated stale/retry handling.
Original SDK adapter and ImGui application remain hardware references; Qt/IPC
is not implemented. The real silent stall is not reproduced.

## Single next task

Complete baseline cloud build/tests and the bounded Phase 1 mock vertical slice;
replace this checkpoint with exact commands/results and Windows handoff before
finishing the session. All physical tests remain UNTESTED ON HARDWARE.
