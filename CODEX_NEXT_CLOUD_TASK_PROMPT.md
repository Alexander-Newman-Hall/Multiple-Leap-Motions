# Paste into a later Codex Cloud session after changes land in GitHub

Continue the Hand Bridge 2.0 modernization in this GitHub repository, using repository state—not prior chat memory—as the source of truth. Start by reading `AGENTS.md`, `PROJECT_SPEC.md`, `PLANS.md`, `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`, `docs/DECISIONS.md`, and latest CI and `docs/HARDWARE_TEST_RESULTS.md`. Report the current branch, commit, latest verified tests and any blockers.

Select the **single next recommended bounded task** from `docs/CURRENT_STATE.md`/`PLANS.md`, unless its prerequisite has failed or become obsolete. Execute it, add deterministic tests and documentation, run all checks available here, and create a reviewable diff/PR. Do not work on fusion/handoff, HTTP/WebSockets, React, or a rewrite. Do not change OSC addresses/case/units/port/default behavior without golden regression tests and explicit compatibility mode. Keep the tracking engine independent of GUI lifetime.

Do not pretend cloud tests validate real Leap hardware or TouchDesigner. At completion update `docs/CURRENT_STATE.md` and `docs/SESSION_HANDOFF.md` with exact tests/commands, evidence, unverified assumptions, changed files and ONE next task that another agent can pick up without chatting with you.
