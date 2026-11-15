# Repository instructions for Codex — Hand Bridge 2.0

Read `PROJECT_SPEC.md`, `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`, `docs/ARCHITECTURE.md`, and `docs/OSC_PROTOCOL.md` before making architectural or output changes. If a file is missing, create an initial version and note what is unverified.

## Product invariants
- Windows x64 / native C++20 / Ultraleap LeapC / independently tracked devices / localhost OSC to TouchDesigner.
- A headless tracking engine MUST continue to run when the optional native Qt control GUI is closed or crashes.
- No React, Electron, Python engine, HTTP server, WebSocket server, fusion, or automatic camera handoff unless explicitly requested in a later milestone.
- Legacy OSC format, units, case, address names, and existing deployed config must not silently change. Test and document migrations.
- Never interpret 'no hands visible' as hardware failure. Distinguish connection, subscription, fresh frames, hand visibility and output health.
- No UI state may bypass the engine's authoritative configuration validation. A change is not 'applied' until the engine acknowledges it.
- SDK hardware tests cannot be declared passed from simulated fixtures alone.

## Engineering workflow
- Read the existing implementation and note filenames before replacing it. Preserve history and recoverability.
- Work in reviewable phases; use a branch/PR and do not force-push. Run the smallest relevant tests after each change.
- Use CMake presets, pinned dependencies, clear error handling, RAII, explicit ownership, bounded queues and structured, rate-limited logs.
- Keep LeapC SDK and real-hardware dependencies out of mock-only tests. Respect binary licenses; do not commit proprietary SDK files.
- Use synthetic/replayed frames to test without a connected camera; add fault injection for service loss, missing frames, queue overrun and OSC failures.
- Run actual builds/tests when the environment supports them; otherwise report precise commands and blocked reasons. Never fabricate success.

## Documentation contract — required after EVERY meaningful change
1. Update `docs/CURRENT_STATE.md`: status by milestone (done / in progress / blocked), tested environment, command, outcome, limitations.
2. Update `docs/SESSION_HANDOFF.md`: branch/commit, files changed, architecture changes, verified tests and their output, unresolved issue, next exact task.
3. When relevant, update: `README.md`, `docs/ARCHITECTURE.md`, `docs/OSC_PROTOCOL.md`, `docs/CONFIG_SCHEMA.md`, `docs/TOUCHDESIGNER_SETUP.md`, `docs/HARDWARE_TEST_PLAN.md`, `docs/DECISIONS.md`, `docs/CHANGELOG.md`.
4. For interface or behavior changes, update matching tests and user instructions in the same PR.
5. Never discard useful legacy README knowledge; move or link it under `docs/legacy/` during cleanup.

## Completion report format
Report changed files; what actually built/ran; test commands and results; hardware tests marked DONE/NOT RUN/BLOCKED; compatibility changes; next action. Prefer evidence, not general reassurance.
