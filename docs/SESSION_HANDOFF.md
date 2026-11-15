# Session handoff — Codex Cloud / Windows PC / next AI agent

**Status:** Initial template. Replace fields below with real run results and preserve prior handoffs in git history or `docs/reports/`.

## Task identity
- Date/time (UTC): 2026-11-15 — template prepared
- Agent / environment: ChatGPT specification author (no build environment)
- Working branch: not created
- Base commit: `054e578525a5d5202ea8f6eb417ce55ded7d61b8`
- Task/milestone: Phase 0 preparation
- PR/issue: none

## What changed

Planning kit only: Codex prompt, AGENTS, specification and supporting documentation. No project source has been modified. This package is not yet in the GitHub repository.

## Verification matrix

| Claim | Environment | Command / Evidence | Result |
|---|---|---|---|
| Repository structure read | GitHub connector | `master` source listing at initial commit | Static inspection only |
| Windows C++ compile | Physical/CI Windows | Not run | NOT RUN |
| Tests | Cloud/Windows | Not run | NOT RUN |
| Two physical cameras | Physical Windows | Not run | NOT RUN |
| OSC receipt in TouchDesigner | Physical Windows | Not run | NOT RUN |
| UI close/restart headless engine | Physical/Mock | Not run | NOT RUN |

## Evidence / warnings

- Static findings in `docs/LEGACY_AUDIT.md` are possible code defects, NOT proof of the silent OSC failure root cause.
- `docs/OSC_PROTOCOL.md` captures partial source-level behavior; must be turned into golden tests before port/address changes.
- UI and IPC descriptions are design targets, not implemented functions.

## Next agent instructions

1. Read `AGENTS.md`, `docs/CURRENT_STATE.md`, `PROJECT_SPEC.md`, `docs/LEGACY_AUDIT.md`, `docs/DECISIONS.md`.
2. Verify the repo commit and current working tree. Do not assume the original master SHA is still current.
3. Write a bounded plan; identify executable/tests; set up mock build as early as possible.
4. Do not remove current functionality to chase UI improvements.
5. Update this file after every task, with exact command outputs and hardware validation status.

## Single next task

**Audit and baseline the existing native C++ repository; document real build/test results and exact OSC/config compatibility.**

---

### Copyable future handoff template

```text
Task ID / PR:
Branch and commit:
Environment and versions:
What was implemented (exact files):
Build commands and result:
Automated test commands and result:
Manual Windows device tests and result:
TouchDesigner receive test and result:
Key logs/metrics and where stored:
Unverified assumptions:
Outstanding defects/blockers:
Docs updated:
ONE next task with acceptance criteria:
```
