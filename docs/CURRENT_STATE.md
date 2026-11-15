# Current project state — living status file

**Updated:** 2026-11-15 (specification kit preparation)
**Project:** Hand Bridge 2.0
**Repo:** https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions
**Initial reference commit:** `054e578525a5d5202ea8f6eb417ce55ded7d61b8` (`master`, observed at planning time)
**Active development branch/commit:** NOT CREATED IN THIS KIT
**Phase:** Phase 0 — awaiting Codex implementation/audit
**Authoritative product spec:** `PROJECT_SPEC.md`

## Reality check

- [x] Product requirements drafted.
- [x] Initial static source assessment (NOT a complete runtime audit).
- [x] Hybrid/cloud/Windows acceptance plan drafted.
- [ ] Reproducible Windows build validated.
- [ ] SDK-free mock pipeline built.
- [ ] CI configured and passing.
- [ ] UI separated from tracking engine.
- [ ] Config migration implemented.
- [ ] Legacy OSC golden tests captured.
- [ ] Real two-camera Windows tracking tested.
- [ ] Reported silent stream stall reproduced and fixed.
- [ ] TouchDesigner OSC receiver verified with the replacement build.

## Current top risks

1. Unknown root cause of reported runtime stall where program runs but output stops; need stage metrics and fault injection.
2. Legacy config alias mismatch and state ownership confusion.
3. CMake/Visual Studio project parity and dependency packaging.
4. Behavioral parity of legacy OSC address/value/unit/zero-on-loss output.
5. Licenses/redistribution rights for SDK/Qt/OSC library.

## Latest verification

This package contains specifications only. The author did not modify the repository, run builds, or test physical cameras as part of this deliverable. Static repository reading does not constitute PASS.

## Single next recommended Codex task

**Phase 0:** Check out current repository, inventory executable paths and legacy OSC/config contracts, create a clean testable baseline with CMake/CI feasibility, document concrete build blockers, and update `docs/SESSION_HANDOFF.md` with exact commands/results. Begin Phase 1 only when baseline behavior is documented.

## How Codex must update this file

Replace this status on every substantive task with:
- UTC date, current branch/commit, phase and active PR
- Items completed since last handoff, changed files and test commands
- Each verification result labeled `CLOUD`, `WINDOWS CI`, `PHYSICAL WINDOWS`, or `NOT RUN`
- Known bugs/risks with issue IDs, blockers and their owners
- **One** next recommended task with acceptance condition
