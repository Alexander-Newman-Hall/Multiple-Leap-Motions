# Hand Bridge 2.0 — Codex project kit

This is a **planning and instruction kit**, NOT a rewritten application. It was prepared for the existing [Multiple Leap Motions repository](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions), using a static source snapshot of the `master` branch at commit `054e578525a5d5202ea8f6eb417ce55ded7d61b8` on 2026-11-15.

## Fast start

1. Open the existing repository in Codex Cloud from your Mac.
2. Paste the **entire** text of `CODEX_MASTER_PROMPT.md` into a new Codex task. This is self-contained and can initiate Phase 0 even if the other files have not been added to the repo.
3. For persistent project intelligence, **add** this kit's `AGENTS.md`, `PROJECT_SPEC.md`, `PLANS.md`, `docs/`, and `configs/` to the **existing GitHub repository** on a feature branch. Do not overwrite the existing `README.md` at first. Codex should preserve original documentation under `docs/legacy/` before modernizing the root README.
4. Start a fresh Codex session after the files are on the branch so `AGENTS.md` is read as repository instructions.
5. Ask Codex to implement one bounded item at a time. Use GitHub Actions Windows for automated native Windows builds/tests and a physical Windows PC with Ultraleap cameras and TouchDesigner for device/OSC validation.
6. At every cloud ↔ Windows handoff, read and update `docs/CURRENT_STATE.md` and `docs/SESSION_HANDOFF.md`, commit results, and attach logs/tests to the relevant PR.

## Top-level files

- `CODEX_MASTER_PROMPT.md`: self-contained first Codex task; **copy this into Codex**.
- `AGENTS.md`: concise persistent instructions automatically read when placed in the repository root.
- `PROJECT_SPEC.md`: full product and engineering requirements, acceptance criteria and scope.
- `PLANS.md`: phased, discrete tasks with milestones and gates.
- `CODEX_WINDOWS_TEST_PROMPT.md`: follow-up prompt for Codex on the physical Windows development/test machine.
- `CODEX_NEXT_CLOUD_TASK_PROMPT.md`: prompt for later cloud iterations.
- `configs/handbridge.example.json`: **proposed v2 JSON** for discussion/testing; not a valid config for the current legacy executable until implemented.

## Documentation index

| File | Purpose |
|---|---|
| `docs/ARCHITECTURE.md` | Engine boundaries, threading, processing and local IPC |
| `docs/OSC_PROTOCOL.md` | Exact legacy OSC compatibility expectations and optional v2 |
| `docs/CONFIG_SCHEMA.md` | Proposed persistent presets and migration rules |
| `docs/RELIABILITY.md` | Reconnection/stall diagnosis, metrics and fault injection |
| `docs/UI_SPEC.md` | Native GUI controls, real state, preview and visual behavior |
| `docs/TOUCHDESIGNER_SETUP.md` | OSC In CHOP/DAT and integration troubleshooting |
| `docs/HYBRID_WORKFLOW.md` | Codex Cloud → Windows CI → hardware PC → cloud |
| `docs/HARDWARE_TEST_PLAN.md` | Physical 2-camera Windows acceptance checklist |
| `docs/HARDWARE_TEST_RESULTS.md` | Living real test results (initially NOT RUN) |
| `docs/LEGACY_AUDIT.md` | Source-level findings and suspected bugs (not proven root causes) |
| `docs/DEPENDENCIES.md` | Toolchain, SDK and legal packaging ledger |
| `docs/TESTING.md` | Synthetic/golden/physical testing strategies |
| `docs/OPERATIONS_RUNBOOK.md` | Proposed headless startup and problem diagnosis |
| `docs/DECISIONS.md` | Architecture decision history |
| `docs/CHANGELOG.md` | Changes that are actually implemented |
| `docs/CURRENT_STATE.md` | Last verified milestone and ONE next task |
| `docs/SESSION_HANDOFF.md` | Exact branch, commands, results, blockers for next agent |
| `docs/SOURCES.md` | Links to official Ultraleap/Qt/Codex docs and reference commit |

## Important boundary

- The native C++ engine and OSC transmission to TouchDesigner are **non-negotiable**; no React/HTTP/WebSocket rewrite. Qt Quick/QML is only the optional native GUI presentation; processing and controls live in C++.
- No camera fusion/handoff in the initial product. Optional common-space calibration for a rigid printed camera frame is allowed.
- A successful cloud build is not proof of real device or TouchDesigner functionality.
- All CLI snippets and the JSON example are **proposed contracts** until implemented and physically validated.
