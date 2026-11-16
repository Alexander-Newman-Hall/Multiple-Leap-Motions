# Hybrid development workflow: Mac → Codex Cloud → GitHub Actions Windows → Hardware Windows PC

## Objective

Keep the owner free to direct and review most software work from macOS, while all platform-specific build and physical camera/TouchDesigner verification occurs in a truthful Windows environment. The repository, commits, issue list and documentation are the shared memory—not a single chat session.

## Work environments and responsibilities

| Environment | Good for | Cannot prove |
|---|---|---|
| Codex Cloud from Mac | Repository audit, C++ code edits, architecture, unit tests, mocks, docs, code review | USB camera health, actual service lifecycle, Qt desktop usability, TouchDesigner receive behavior |
| GitHub Actions `windows-*` | MSVC/CMake builds, regression tests, ABI/packaging checks when dependencies available, upload artifacts | Physical two-camera tracking or service unplug/replug without dedicated attached hardware |
| Windows PC with physical controllers | Hyperion integration, 2-camera USB setup, real UI, real OSC In CHOP/DAT, unplug/restart/soak/perf | Unreviewed source changes in cloud (must return findings through repo) |

## Standard iteration loop

1. Read `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`, open issues and latest CI results before starting.
2. Select **one bounded milestone/subtask** (e.g. add per-device metrics, not “finish the app”). Branch from the current integration branch and link work to an issue or task identifier.
3. Codex reads `AGENTS.md`, `PROJECT_SPEC.md`, relevant docs and concrete source files. Implement, compile where possible and run deterministic tests.
4. Record exact commands and results. For non-Windows/cloud jobs say `UNVERIFIED ON WINDOWS` or `UNTESTED ON PHYSICAL CAMERA`; never replace these with a green status based on a simulator.
5. Push PR, await Windows CI, review test reports and fixes. Artifacts are versioned and traceable to commit SHA.
6. On a Windows PC, check out the tested commit, install matching SDK, run smoke/real-camera tests and record exact results to `docs/HARDWARE_TEST_RESULTS.md` and the issue/PR.
7. Commit Windows results and logs (redacted), feed results into the next Codex task. Update `docs/SESSION_HANDOFF.md` with single recommended next task.

## First-time setup: cloud

- Connect GitHub repository `Alexander-Newman-Hall/Multiple-Leap-Motions` to Codex and publish a project environment with CMake and a C++ toolchain. Most cloud tasks can run with mock frame source and no SDK.
- Copy this kit's `AGENTS.md`, `PROJECT_SPEC.md` and `docs/` to the repository on a new branch. Do not overwrite current README initially; Codex should create a clean README while preserving the original as a legacy reference.
- Paste `CODEX_MASTER_PROMPT.md` into the first task. Codex should start with Phase 0, not attempt every phase in a single giant commit.
- Do not place Ultraleap licensed binaries, license keys, machine-specific SDK paths or sensitive logs in the repository.

## First-time setup: Windows development/validation machine

- Windows 10/11 x64, current supported MSVC/Visual Studio Build Tools + Windows SDK, CMake, Git, Qt 6 matching project lock, Ultraleap Hyperion tracking software and LeapC development files per official instructions, two properly provisioned controllers, TouchDesigner.
- Prefer independent USB 3 controller/ports as recommended by device guidance; record ports and hub/controller topology when debugging.
- Document installation commands and version numbers in `docs/DEPENDENCIES.md` / `docs/HARDWARE_TEST_RESULTS.md` instead of storing them in chat.
- Clone repository; select the exact PR/commit build, run CMake preset, unit/integration tests, and then real-device smoke test using `docs/HARDWARE_TEST_PLAN.md`.
- Before pushing logs, redact device serials where needed and avoid including private home paths, tokens or system-level identifying information.

## CI design

- Windows workflow should have separately labeled targets such as `core-mock-tests`, `sdk-integrated-build` (only if licensed SDK available), `gui-build` (with Qt set up), and `package` (when permitted).
- An SDK-free mock build may be shipped as a developer fixture only; **do not label it a working hardware application**.
- Cache downloaded dependencies cautiously, pin versions/SHAs and use reproducible CMake presets. Validate third-party licenses/redistribution before embedding binaries.
- Publish test result summaries in CI; leave real hardware matrix as separate manual checks.
- Keep release packages as signed/unsigned provenance-labeled artifacts; do not claim signed installers unless code signing is configured.

## Windows integration reports

Every physical hardware report must contain: commit SHA, Windows version, SDK/service version, controller models/count, port info, config/preset, TouchDesigner build/version, exact run commands, logs/metrics, pass/fail per test, and open issues. Attach a short OSC In DAT or CHOP test log/screenshot, with actual receiver observation.

## Handoff protocol

A new Codex session starts by saying in its internal plan: **"Read current state, handoff, decisions, latest CI and test results; do not trust prior conversational assumptions."** On completion it updates these source files. The docs stay small and current; archived reports and design history live under `docs/reports/` or committed PRs.

See `docs/SESSION_HANDOFF.md` and `docs/CURRENT_STATE.md` templates. The cloud task may not directly access Windows physical hardware. Never say "verified on hardware" if it merely obtained GitHub Actions green tests.

## Current handoff entry point

The Windows implementation is on `codex/handbridge-leapc-release`, with validated
source `c8596f2` and handoff `2c1ce8e`; earlier branches remain preserved. Its real
Hyperion6.2 adapter and Release package have CI evidence, but physical Windows
acceptance is still open. `windows-legacy` remains the original GUI reference.

The user has now requested Mac-port planning on `codex/handbridge-macos-plan`.
Read [MACOS_PORT_PLAN](MACOS_PORT_PLAN.md), CURRENT_STATE and SESSION_HANDOFF before
starting. Use a fresh local Mac Codex chat for M0 SDK/toolchain/probe work, followed
by actual two-device feasibility. A cloud Mac runner can compile/mock-test a port;
it cannot observe the owner's USB cameras. Selecting a new chat does not change
which computer its commands execute on. Record platform and test class separately.
