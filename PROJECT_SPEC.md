# Hand Bridge 2.0 — Product and Engineering Specification

**Status:** Authoritative v1 specification; Phase 0 code audit completed 2026-11-15. Implementation status is maintained in docs/CURRENT_STATE.md; requirements below are not claims of shipped features. **Owner:** Repository owner. **Target:** Windows 10/11 x64. **Product:** Native multi-Ultraleap camera tracking bridge for local TouchDesigner OSC workflows.

**Mac platform extension (2026-11-16):** the user selected **Apple Silicon only,
native arm64**; see [MACOS_PORT_PLAN](docs/MACOS_PORT_PLAN.md). Intel Mac builds,
Rosetta validation and universal releases are outside the planned product scope.
Windows x64 remains the existing release target. Historical Intel SDK/host checks
are reference evidence only and cannot satisfy arm64 acceptance. C++20, Hyperion
**6.2.0 or newer within 6.x**, the existing Qt 6.8.3 GUI, OSC parity and independent
engine lifetime remain requirements. Native arm64 engine/GUI/SDK results and
physical two-camera gates must be recorded before support is claimed.

## 1. Product purpose

Hand Bridge is an installable Windows utility to obtain **independent** tracking from two or more supported Ultraleap/Leap Motion controllers on a single Windows computer, select and optionally process per-hand data, and transmit it to **TouchDesigner on the same PC over OSC/UDP**. The interactive artwork, graphics and user gestures may be implemented in TouchDesigner; Hand Bridge must remain a predictable, reliable hardware-data source.

The software must operate headlessly in a production installation. A separate graphical control application provides setup, live health information, per-camera configuration, filter preview, presets, and diagnostics. UI startup, shutdown, crashes or low frame rate cannot interrupt tracking or OSC.

## 2. Goals, non-goals and design boundaries

### Goals (ordered by importance)
1. Correct, sustained multi-camera LeapC tracking under device/service interruptions.
2. OSC backward compatibility with existing TouchDesigner projects.
3. Headless, independently supervised engine; optional polished native control UI.
4. Real processing/output controls with persistent, observable applied state.
5. Reproducible Windows builds, synthetic test source, useful diagnostics and maintainable documentation.
6. Rig-ready optional shared-world transforms for 3D-printed mounts, WITHOUT requiring fusion.

### Out of scope for initial stable release
- Cross-camera joint fusion, blending, handoff or same-person identity stitching.
- Tracking multiple cameras on macOS as a release requirement.
- Machine-learning inference or raw IR computer vision.
- Cloud services, accounts, remote telemetry, cloud cameras, HTTP, WebSockets, React, Electron, Python runtime.
- Unreal/Unity plugins, custom graphics rendering engine, image camera feed capture.
- Windows Service installation with elevated privileges, unless validated as necessary for deployment.

## 3. Operating constraints
- Primary release: Windows 10/11, MSVC, x64, C++20, CMake presets. Mac extension: native arm64 Apple Silicon only; no Intel, Rosetta or universal-release requirement. Ultraleap Hyperion 6.2+ within 6.x and compatible Leap Motion Controller 2 devices remain external test inputs.
- IPC: local-only native mechanism. Recommended implementation: Qt 6 `QLocalServer`/`QLocalSocket` between headless host and Qt native GUI; core tracking/processing can remain Qt-independent. Explicitly restrict local control endpoint access and protect against a second engine instance.
- UI: Qt 6 Qt Quick/QML + C++ models (a native executable, **not a browser app**). Keep UI as a distinct process; import into engine only a small IPC bridge, not visual modules.
- OSC: UDP to `127.0.0.1:<configured-port>`; current source uses a 9000 fallback in `src/core/ConfigManager.cpp`; existing saved configs/UI may differ, so document exact compatibility after audit. No internet connection required.
- Use stable but human-readable aliases assigned by device serial. Device IDs from the SDK may be transient.
- All public data fields must have documented units, axes, validity and lifecycle.

## 4. Functional requirements

### FR-01: Camera discovery and lifecycle
- Create a multi-device-aware LeapC connection using the installed SDK's supported documented interfaces; handle device-connected/lost and subscription/unsubscription events, initialization failure and cleanup.
- Discover connected devices; expose serial (safely and optionally redacted in exported diagnostics), alias, SDK ID, connection and subscription state. Handle duplicate/transient events without leaked handles or duplicate OSC streams.
- Keep camera streams independent. A problem in A must not halt B. Support reconnect, unplug/replug, service restart, app restart and device enable/disable.
- Check SDK-specific behavior against real headers and official Multi Device Example before implementation.

### FR-02: Tracking data model
- Frame: source device serial, alias, SDK frame ID (if available), SDK timestamp and arrival timestamp, unit/space metadata, frame validity and sequence.
- Hand: SDK hand ID (if exposed), handedness, tracking present/valid, confidence where provided, palm position/orientation/normal/velocity, wrist, finger bones and tips, pinch/grab strengths, time visible. Do not fabricate unavailable fields.
- Carry RAW observed data separately from PROCESSED values and OUTGOING serialized samples.
- Use a canonical internal coordinate space with explicitly documented units (recommended meters internally, raw SDK millimeters captured faithfully). Preserve raw-mm OSC in legacy mode.
- Support a future device-to-world rigid transform per camera without requiring multi-camera association or fusion.

### FR-03: Reliable, diagnosable streaming
- Keep separate liveness signals for: service, device, event subscription, frames, hands visible, queue processor, OSC socket/send. Receiving empty-hand frames means camera is alive.
- Per-device indicators: last frame age, input FPS, frame queue depth/drop count, current hands, reconnect attempts, last error, last successful subscription.
- Per-output indicators: configured target, serialization FPS, UDP packet/s counts, errors and last successful OS send call. A successful UDP send does NOT prove TouchDesigner received it.
- State machine: DISABLED → CONNECTING → SUBSCRIBED → STREAMING; failure branches DEGRADED / RECONNECTING / DISCONNECTED. Define exact transitions, guard conditions and recovery actions. No busy-waiting, unbounded retries, races or resubscriptions with stale handles.
- Watchdog distinguishes connection loss vs no hands; stale frames trigger diagnostics and controlled reconnect only when justified. On loss, emit validity/loss according to output contract; do not keep a stale hand as live indefinitely.
- Avoid flooding the logs with a line per joint or OSC packet; use levels, sampling/rate limits and an exportable rolling diagnostic buffer.

### FR-04: OSC output to TouchDesigner
- Legacy output mode is DEFAULT and supports the existing per-device/per-hand OSC address family, data types, casing and units. Capture actual golden fixtures from source/tests and validate against a TD receiver. See `docs/OSC_PROTOCOL.md`.
- Default destination loopback; configurable IP, port and independent send rate. Enable/disable output globally or by device/hand/channel. Avoid silent disablement.
- Optionally packetize several OSC messages in a time-consistent bundle, but honor the maximum safe UDP payload and provide an unbundled compatibility setting. One frame's data should have a coherent capture timestamp/sequence where supported.
- Coalesce or downsample without blocking LeapC; prefer dropping old unsent frames to growing latency indefinitely. Capture samples before serialization, and avoid logging every individual value.
- Distinguish tracking validity from actual `(0,0,0)` coordinates. In legacy mode preserve old loss behavior unless explicitly changed; make v2 validity explicit.
- Document an opt-in OSC v2 format once implemented, with versioned address schema, typed arguments and migration guidance. Do not replace legacy with v2 by default.

### FR-05: Selectable data and processing
- Device enable, assigned handedness (`left`, `right`, `both`, `none/disabled` semantics clearly defined), channel selectors: palm, wrist, each fingertip, optional all joint bones, orientation, velocity, normal, pinch, grab, extension, visible time, validity.
- Optional per-channel processing: coordinate system transform (camera/world), global/device offset and rotation, normalized mapping, scaling, sensitivity, exponential or One-Euro time-based smoothing, speed-dependent smoothing where appropriate, dead zone, clamp, output unit. Show exact operation order and defaults. No processing means unaltered data.
- Separate user-defined gesture detection with hysteresis and explicit thresholds; do not confuse LeapC pinch strength with a discrete pinch event. New detectors must be optional.
- Per-device/per-channel override inheritance must be documented. Never normalize to 0–1 without showing ranges and clipping. No hidden axis swaps.
- Changes update the engine atomically and must be reflected in outgoing OSC; GUI gets acknowledgement/rejection and revision. A visible UI slider alone is NOT proof of applied behavior.

### FR-06: Configuration and presets
- Versioned JSON files with explicit schema version, defaults, validation, migration, and atomic write to temporary file then rename/backup. Never expose corrupted partial files after crash.
- Stable persistence of serial→alias map and saved hand assignment; migration must handle discovered original `deviceAliases` vs `device_aliases` serialization inconsistency.
- Save/load/export/import named profiles: default two-camera installation, minimal index-fingertip, all joint debugging, raw output. Do not include device-specific serials in shareable templates without explicit export choice.
- A headless process must load and apply settings without opening a UI. CLI `--config`, `--headless`, `--validate-config`, `--version`, `--help`; exact CLI design to be implemented/documented, not assumed present now.
- Detect invalid port, transform, numeric range, duplicate alias and missing/renamed device; show actionable errors, preserve last good config where safe.

### FR-07: Graphical control application
- Native, keyboard-accessible, performant dark UI; high DPI, resizeable; clear state and units; text/slider input where appropriate; reset per control/module; good alignment and legible contrast.
- Sections: (a) Overview, (b) Devices, (c) Processing, (d) OSC Output, (e) Profiles & Diagnostics. See `docs/UI_SPEC.md`.
- Two independent camera tiles with real connection/subscription/frame/hand/OSC diagnostics, not fabricated UI counters.
- Optional 3D hand skeleton preview with camera axis gizmos, world transforms and raw vs processed hand geometry; throttle to e.g. 30Hz or less, independent of capture rate. If this is expensive, implement 2D/coordinate preview first.
- Control validity and temporary feedback: Unmodified / Editing / Applied / Rejected / Saved; UI reconnect to running engine and refresh full state; safe when engine is absent.
- GUI may be run multiple times or blocked by a single-controller policy, clearly documented. Closing window never stops engine.

### FR-08: Simulation and diagnostics
- `ITrackingSource` abstraction with real LeapC implementation and synthetic/replay implementation (same internal frame types). No need for physical cameras to run core tests.
- Deterministic fixtures for two cameras and no hands, stable left/right, tracking dropout, service loss, duplicate events, restart and queue overload.
- Fault injection can simulate output socket errors, stale stream and reconnect. Replayer makes CI and remote debugging possible.
- Diagnostics export includes version, OS, SDK metadata if available, relevant status transitions and config with sensitive paths/serials redacted by default; no recording of raw camera images by default.

### FR-09: Lifecycle & packaging
- Separate distributable `handbridge-engine.exe` and `handbridge-control.exe` using one signed/unsigned installer when feasible. A lightweight desktop/tray launcher is optional.
- A single engine instance per user/session is recommended; lock and IPC errors handled cleanly. Do not try to restart the OS tracking service automatically without security considerations.
- Support clean Ctrl-C/shutdown, connection/device cleanup and last-valid config persistence. Crash report/logging is local-only.
- Clearly distinguish application background process, login autostart, and Windows Service; do NOT install the latter automatically.

## 5. Performance and quality targets

All numeric targets below are **provisional acceptance budgets**, to measure on the target Windows PC and revise after profiling, not claims about current performance:
- Do not block the LeapC polling thread on GUI rendering, disk I/O, network retries or per-joint logging.
- Prioritize freshest observations; bounded queues and documented policy on overload.
- Output rate configurable, with default aligned to real sensor capability rather than assuming a specific FPS. UI previews throttled independently.
- No memory/handle leaks during repeated 100-cycle simulated connect/disconnect tests.
- 1-hour unattended hardware soak initially, followed by 8-hour deployment soak when available; no unnoticed freeze or stale pose. Record actual dropped frame counts and OS/SDK versions.
- Report measured end-to-end latency (camera frame arrival → OSC serialization/send and optional TouchDesigner receive); do not invent a numeric guarantee.

## 6. Proposed milestone plan and acceptance criteria

### Phase 0 — Evidence & baseline
- Map current code/entrypoints/build; separate real paths from abandoned ones; run current tests if possible. Inventory OSC addresses, defaults, config locations, third-party dependencies/licensing, CI feasibility and hardware requirements.
- Save original README as accessible legacy reference before editing. Capture current known bugs and clearly separate proven code defects from runtime hypotheses.
- Produce baseline `docs/LEGACY_AUDIT.md`, `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`, `docs/DECISIONS.md`.
**Gate:** a reproducible inventory and exact next task, no false build claims.

### Phase 1 — Reliable headless foundation
- Bring up testable C++ core with mock frame source and local OSC transmitter; standalone headless process; decouple from UI. Repair build and config alias bug; gold-test legacy OSC, basic frame validity and selective output.
- Establish mock reliability instrumentation; state-machine tests; controlled reconnect foundation; empty-hand and loss semantics.
**Gate:** CI builds/tests SDK-independent path, two mock camera streams → OSC, GUI can be absent.

### Phase 2 — Hyperion hardware adapter and Windows integration
- Implement verified LeapC multi-device API from installed headers and official documentation, proper handle/subscription lifecycle, service/device recovery, USB stale stream diagnostics. Add hardware-specific Windows build instructions.
**Gate:** real two-camera tracking and recovery verified by documented Windows test results, OR marked BLOCKED ON HARDWARE.

### Phase 3 — Native control UI + IPC
- Qt 6 native GUI and local-only control link; Overview/Devices/OSC/Diagnostics functional before advanced visual polish. Settings changes acknowledged by engine; UI can exit/restart independently.
**Gate:** data continues after GUI close/restart, testable without cameras via simulator.

### Phase 4 — Processing and profiles
- Full data/channel controls, raw/processed visualization and optional smoothing, calibration transforms, named presets, migration tests.
**Gate:** changes deterministically alter simulated OSC output and persist after restart.

### Phase 5 — Hardening and release
- Soak/fault tests, TouchDesigner examples, Windows installer/zip packaging, license review, build/release documentation, measurable performance/latency.
**Gate:** hardware validation checklist plus release notes documenting any remaining limitations.

## 7. Delivery artifacts
- Root `README.md`: product purpose, quick start Windows/TouchDesigner, install/run headless/gui, known limitations, doc index.
- `AGENTS.md`, `PROJECT_SPEC.md`, `docs/ARCHITECTURE.md`, `docs/OSC_PROTOCOL.md`, `docs/CONFIG_SCHEMA.md`, `docs/UI_SPEC.md`, `docs/TOUCHDESIGNER_SETUP.md`, `docs/HYBRID_WORKFLOW.md`, `docs/HARDWARE_TEST_PLAN.md`, `docs/LEGACY_AUDIT.md`, `docs/DEPENDENCIES.md`, `docs/DECISIONS.md`, `docs/CHANGELOG.md`, `docs/CURRENT_STATE.md`, `docs/SESSION_HANDOFF.md`.
- CMake presets, Windows CI workflow, production/mock targets, tests, example profiles and matching screenshots/preview fixtures where useful.

## 8. Primary risks, mitigations and open questions
1. **SDK packaging/licensing and headless CI:** do not assume CI runner has proprietary Ultraleap libraries. Use mock mode; document genuine hardware build requirements.
2. **Silent output stalls:** may originate in LeapC service, subscription state, polling, frame queue, processing loop, logging or OSC. Instrument each boundary; diagnose with counters and fault injection before claiming cause.
3. **Legacy OSC compatibility:** existing TD networks may rely on float-per-address, zeros on hand disappearance, and current serial aliases; maintain a compatibility mode and explicit migration examples.
4. **UI coupling:** the old code associates main-loop frame processing and UI. Remove that dependency before visual redesign.
5. **3D-printed physical rig:** fixed relative camera transforms make a calibration UI useful; fusion is not required to benefit from such a rig.
6. **Qt licensing/deployment:** verify Qt version and applicable license requirements before redistributing binaries. Prefer permitted dynamic linking/packaging, preserve notices.
7. **Cloud tests ≠ field tests:** require local Windows results for actual USB/Hyperion/TouchDesigner claims.

## 9. External authoritative references
- https://docs.ultraleap.com/hand-tracking/Hyperion/index.html
- https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html
- https://doc.qt.io/qt-6/qtquick-index.html
- https://doc.qt.io/qt-6/qlocalserver.html
- https://docs.github.com/en/actions
- https://learn.chatgpt.com/docs/agent-configuration/agents-md
