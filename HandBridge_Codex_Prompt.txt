# Paste this entire prompt into Codex (initial task)

You are the lead C++ systems engineer, Windows build engineer, test engineer, and technical writer for **Hand Bridge 2.0**.

Repository: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions
Current default branch at the time this brief was prepared: `master` (reference commit `054e578525a5d5202ea8f6eb417ce55ded7d61b8`; recheck before edits).

## Product goal
Modernize the existing native Windows application that receives tracking streams from **multiple Leap Motion Controller 2 devices through the Ultraleap LeapC API** and sends selected/processed hand tracking values to **TouchDesigner on the SAME Windows PC using OSC over UDP (localhost)**. A native optional control application should make device configuration, data processing, diagnostics, and OSC output easy to understand and control. The tracking engine MUST run independently, without a GUI, and keep working when the GUI closes.

## Non-negotiable decisions
- Native **C++20** throughout the engine. Windows 10/11 x64 first. CMake/MSVC builds. Do NOT rewrite the engine in Python, Rust, C#, JavaScript, React, or Electron.
- Keep LeapC/Ultraleap Hyperion as the actual hand tracking provider. Consult the **installed SDK's exact headers** and official current docs; do not invent API symbols. Do not attempt to implement neural-network hand tracking from camera images.
- Preserve **independent per-camera tracking**. No joint fusion, cross-camera hand matching, or automatic handoff in the initial release. Leave a modular extension seam for these, but do not add experimental complexity until reliability is demonstrated and explicitly requested.
- **OSC/UDP is the main TouchDesigner data output**. Default destination should be `127.0.0.1`; preserve the currently deployed legacy address patterns, case, values and default behavior as far as possible. IMPORTANT: current `ConfigManager.cpp` uses fallback `osc_port=9000`, while some earlier examples mention other ports. Audit actual source, saved settings, UI, documentation, and tests and record the exact compatibility contract before changing anything. Do NOT silently change the port or OSC address scheme. Introduce a versioned v2 format only as an opt-in alternative.
- Separate the always-running tracking/processing/OSC engine from an OPTIONAL native Windows control application. Preferred UI: **Qt 6 Qt Quick/QML with C++ models/controllers**; QML is an in-process native UI technology, NOT React or a web server. No HTTP or WebSockets. Use local-only native IPC (e.g., QLocalServer/QLocalSocket, with Qt Core/Network only in the headless host) for GUI commands and low-rate status/preview. Document local IPC schema and access restrictions. If a simpler Qt Widgets/C++ UI demonstrably reduces risk, make a short architecture decision record before substituting it.
- The engine's data path must NOT depend on the control window's lifecycle, event loop, frame rate, or responsiveness.
- Target a standard Windows desktop app + headless console/process mode; do not introduce an elevated Windows Service unless strictly required and approved. Auto-start at login can be a separate installation option.
- Do NOT delete the original source/README or make a wholesale rewrite without a baseline audit, migration plan, parity tests, and a rollback path.
- No false claims: a successful cloud compile with simulated frames is NOT proof that actual Ultraleap hardware, USB hotplug, Hyperion, or TouchDesigner worked. Mark these **UNTESTED ON HARDWARE** until tested on a physical Windows PC.

## Build philosophy and execution instructions
1. Inspect the COMPLETE repository first: CMake and Visual Studio projects, src/core, src/pipeline, src/app, src/ui, transport, tests, README, dependencies and tracked build artifacts. Identify the actual running code path, dead/duplicate paths, and config/OSC compatibility.
2. Read and obey the attached files if present: `AGENTS.md`, `PROJECT_SPEC.md`, `PLANS.md`, and `docs/*.md`. If not present, CREATE these documents from the requirements in this prompt before substantial refactoring. Keep instructions in AGENTS concise, detailed designs in `PROJECT_SPEC.md` and `docs/`.
3. Create a feature branch / isolated working branch, protect the original branch, and structure changes as small reviewable milestones. Prefer incremental modernization to a big-bang rewrite. Do not force-push or delete original work.
4. Begin with Phase 0 (audit, baseline, documentation, compile/test feasibility). Continue into Phase 1 (headless/mock pipeline + tracking liveness/recovery + OSC parity + reproducible Windows build) if feasible. For every further phase, implement and verify only after the preceding acceptance criteria are met. Do useful independent work without blocking on unnecessary questions.
5. If the SDK is missing in cloud CI, build and test all SDK-independent components with a fake frame source. Provide a clear Windows SDK installation/build path for the hardware-integrated target. Do not download, vendor, or redistribute proprietary SDK binaries without confirming license and permissions. Prefer CMake presets with explicit `LEAP_SDK_ROOT`, pinned dependencies, and clear diagnostics.
6. Set up GitHub Actions on a Windows runner for feasible CMake builds, static checks, and hardware-independent tests. Publish CI artifacts when legally/technically possible, but do not call a stub-only binary a working full product. Do not place cached local build outputs or binaries under version control.
7. Write tests before replacing legacy behavior, especially for OSC messages, configuration migration, and device lifecycle. Add deterministic replay and fault-injection tests to debug the reported failure where the application remains alive but OSC stops.
8. When changing an output protocol or persistent configuration, create golden fixtures/migration tests and update `docs/OSC_PROTOCOL.md`, `docs/CONFIG_SCHEMA.md`, and `docs/TOUCHDESIGNER_SETUP.md` in the same change.
9. Maintain documentation throughout the project, not only at the end: a clean root `README.md`, concise `AGENTS.md`, `PROJECT_SPEC.md`, and docs for architecture, dependencies, hybrid workflow, Windows testing, decisions, and living handoff state. Preserve the existing detailed README in `docs/legacy/` or another recoverable location and link to it; do not throw away useful implementation notes.
10. At the end of EACH coding task/PR, update `docs/CURRENT_STATE.md` and `docs/SESSION_HANDOFF.md` with exact commits/branch, changed files, build/test commands with results, what is verified vs assumed, known bugs, SDK/hardware blockers, and the **single next recommended task**. Keep this up to date for Codex Cloud → GitHub Actions → Windows PC → Codex Cloud handoffs.

## Required engine capabilities
- Multiple devices: proper multi-device-aware LeapC connection, stable serial→alias mapping (`dev1`, `dev2`, etc.), subscription/unsubscription, reconnect/re-enumerate after service or device interruption, lifecycle cleanup, per-device enable/disable, left/right filtering.
- Fault isolation: failure of one device does not silently block the other. Distinguish service connected, USB/device connected, subscribed, tracking frames fresh, hands visible, processing alive, OSC socket available. No-hands is not a device failure.
- Instrumentation: per-device last frame age, input FPS, queued/processed frames, drops, connection/retry counts, last error and timestamps; output FPS, serialized message/bundle counts, UDP send errors and destination. NEVER claim 'OSC received' without receiver-side acknowledgement/verification.
- Automatic recovery with a documented state machine, bounded timeouts and capped retry/backoff. Detect service loss, stale frames, queue stalls, failed subscription and send errors. Discard or mark stale data invalid; don't freeze the last pose indefinitely. A user should be able to manually restart a device or output from UI/CLI.
- Data model: preserve device serial/alias, frame ID/timestamps, native hand ID, handedness, validity, confidence (where exposed), palm, wrist, fingertip/bone/joint data and pinch/grab. Clearly distinguish raw vs processed vs output values, and mm vs m.
- Processing controls (per profile/device/hand/channel where appropriate): enable/disable, raw pass-through, smoothing (time-based), dead-zone, coordinate transform, unit conversion, scaling/offset, output frequency, optional explicit validity, configurable pinch/grab thresholds and hysteresis. Do not create arbitrary motion when tracking disappears. All applied settings must affect REAL OSC output, not just UI appearance. Avoid hidden smoothing/latency by default.
- OSC: original address structure and data types maintained in legacy mode; performance improvements should be internal or optional. Add packet/bundle support respecting UDP MTU considerations, documented packet boundaries, and deterministic output tests. Support user-configured localhost IP/port and channel toggles. Provide optional v2 frame-oriented output only if thoroughly documented and opt-in.
- Configuration: versioned JSON, schema validation, migration from legacy config, persistent aliases and hand assignments, atomic save with backup, transactional apply/reject reporting, defaults, named presets, startup restore; working headless with no GUI.
- Simulator/replayer: deterministic synthetic camera frames and saved sanitized fixtures; ability to simulate USB disconnect/reconnect, service loss, no hands, queue overflow, OSC destination failure and frame stall WITHOUT connected Leap devices.

## Required UI (optional controller, not on tracking critical path)
Five main areas: **Overview, Devices, Processing, OSC Output, Profiles & Diagnostics**. Include a clear live two-camera status view, last-frame age and recovery state, a throttleable hand skeleton/coordinate preview if feasible, precise parameter controls (units, range, reset, enabled state), and explicit Saved/Unsaved/Applied/Rejected status. Support changing device hand assignments, selected joints, smoothing, thresholds, transforms, raw/processed selection, OSC port/address mode, destination and send rate. Include a read-only log/diagnostics panel and diagnostic export. Closing/crashing the UI must not interrupt OSC or tracking. No web server, external account, or internet dependency.

## Suggested target modules (adapt to actual codebase)
- `handbridge-domain`: device/frame/hand/output types; timestamps and units; configuration schema.
- `handbridge-tracking`: `ITrackingSource`, LeapC adapter, mock/replay adapter, reconnection state machine.
- `handbridge-processing`: validated transforms, filters and channel routing.
- `handbridge-osc`: serializers and UDP sink, legacy/v2 adapters.
- `handbridge-runtime`: headless lifecycle, supervised bounded queues, metrics, config.
- `handbridge-control-protocol`: local commands/telemetry DTOs and IPC adapter.
- `handbridge-engine.exe`: headless host.
- `handbridge-control.exe`: Qt 6 native optional GUI.

Avoid coupling rendering, file dialogs, or GUI callbacks into the tracking modules. Use explicit ownership, thread boundaries, RAII and clean shutdown. Prefer small testable units over an excessive abstraction framework.

## Test/acceptance gates
- Windows x64 build and repeatable CMake presets. Core/mocks/OSC tests work in CI with no Leap device; hardware integrated build available on a properly configured Windows SDK machine.
- Two simulated devices operate independently; one device can go stale/disconnect/reconnect without affecting the other; duplicate serials and restart cycles handled deterministically.
- OSC legacy golden tests assert exact addresses, argument types, values, case, and selected default port; TouchDesigner OSC In CHOP/DAT smoke test is documented.
- Config save/reload/migration round trips preserve aliases, hand assignments, channel filters, ports, transforms and presets; invalid inputs are rejected safely.
- UI attached/detached/restarted while engine runs: OSC continues; settings apply and persist; UI shows actual engine state.
- Fault-injection verifies each failure is visible, logged once per meaningful transition, and recoverable after the underlying fault is removed. Last pose is not silently held forever.
- Windows hardware acceptance checklist covers two USB devices, unplug/replug, tracking service restart, no hands, long-run soak, CPU/latency, TouchDesigner receipt, multiple output profiles, app reboot, UI close/reopen. Mark physical tests blocked/unverified when not executed.
- Acceptance results and remaining limitations are written in docs before declaring a milestone complete.

## Required first deliverable
Create/refresh documentation and a prioritized engineering implementation plan based on code-level evidence, with filename/line references. Fix the build/test setup and at least one highest-priority reliability issue if possible. Do NOT pretend all phases are finished in one response. Summarize: (a) confirmed existing behavior, (b) probable causes of silent OSC loss vs unproven hypotheses, (c) branch/commit, (d) files changed, (e) Windows/cloud commands + real test results, (f) outstanding physical tests, and (g) exact next Codex task. Execute rather than only proposing, within available tools and permissions.

Reference links (check actual versions):
- Ultraleap Hyperion: https://docs.ultraleap.com/hand-tracking/Hyperion/index.html
- LeapC multi-device example: https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html
- Qt Quick: https://doc.qt.io/qt-6/qtquick-index.html
- Qt local IPC: https://doc.qt.io/qt-6/qlocalserver.html
- Codex AGENTS.md: https://learn.chatgpt.com/docs/agent-configuration/agents-md
