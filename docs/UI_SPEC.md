# Native control application — UI/UX specification

**Status:** Native Qt6 Quick/QML controller implemented on branch
codex/handbridge-leapc-release (built on the preserved native-controller branch);
actual cloud-native screenshots and test
evidence are recorded in CURRENT_STATE. Hardware integration remains untested.
The requirements below still include later features, not all shipped controls.

## Implemented controller

Five pages share a matte charcoal VST-style frame, mint/violet signal accents,
editable rotary controls, keyboard navigation and engine-backed health. Global
Apply/Revert separates draft editing, acknowledged application, rejection and
persisted profile state. Views do not own the engine.

Overview shows two independent live camera tiles, measured frame/packet rates,
frame age/drop counts and raw/processed measured palm/wrist/tip previews.
The tabletop preview projects X horizontally and positive Z downward in each
sensor's panel. With fingers pointing away, fingertips appear above the palm
and wrist. Raising a flat hand changes the displayed Y coordinate while its
top-down footprint stays in place. Processed meters use the same display scale
as the equivalent millimeters. Devices
controls aliases, per-camera mute and handedness plus actual restart. Processing
controls bypass, smoothing/deadzone, scale/XYZ rotation/offset/units and optional
pinch/grab hysteresis. OSC Output controls target, enable/rate and all14 supported
channel flags. Profiles & Diagnostics saves/loads applied settings, exports
redacted diagnostics and displays real bounded logs.

Direct typing then clicking Apply commits valid focused editors. Numeric parsing
uses an explicit consistent locale. Dotted serials are literal identifiers.
Editor controls disable during transactions and backend prevents lost edits during
profile loads. Offline views are labeled/disconnected, with separate real-engine start,
simulator start and reconnect actions; no activity is fabricated. All current
editable schema fields are represented. Full per-channel processing overrides,
a full skeletal/3D renderer, OSC bundles/v2 and a full installer remain future
work. The real LeapC source is an optional Windows build whose physical
acceptance remains unverified.

### Real-engine startup and SDK diagnostics

When disconnected, **Start engine** selects the real LeapC provider and **Start
simulator** explicitly selects synthetic cameras. Reconnect attaches to an
already-running engine. The real-engine action probes the installed SDK
asynchronously; the interface remains responsive and startup/settings controls
respect the busy state. A failed SDK/runtime/version check returns an actionable
error rather than silently falling back to simulation. A successful probe then
launches the engine detached, so closing the controller does not stop tracking or
OSC.

The runtime is an external dependency: install compatible Ultraleap Hyperion and
LeapC development/runtime files separately. SDK headers and binaries are not
included with the controller or committed in this repository. The optional real
build requires the supported Hyperion/LeapC v6 line: both the client API and the
connected tracking service must be **6.2 or newer within 6.x**. A DLL file-version
string alone is not proof of compatible client or service behavior. The service
version is checked after establishing the LeapC connection before the source can
report healthy tracking.

Provider labels follow actual engine telemetry. **SIMULATED SOURCE** appears only
for a simulated source; **LEAPC SOURCE** identifies the real provider without
claiming camera or receiver validation. **UNTESTED ON HARDWARE** remains explicit
until physical acceptance is recorded.

Overview displays a wrapped **Last tracking error · retained history** message
when the source reports an error. This message can remain after recovery and is
not itself a claim of a current fault. Profiles & Diagnostics separately shows:

- **Library version/path:** the loaded DLL's file metadata and resolved location.
- **Tracking service:** the actual `LeapGetVersion(ServerLibrary)` version, or
  **Not connected / unverified**; it is not copied from the DLL file version.
- Connection attempts, poll errors and the retained last tracking error.

Preflight reports expose the `LeapGetVersion(ClientLibrary)` client API version
separately from DLL file metadata. Device freshness and hands visible still come
from tracking events; local IPC connection or successful DLL loading alone does
not establish fresh physical tracking. The GUI has no ownership of SDK handles,
processing timing or the engine's lifetime. Qt Quick remains confined to the
controller process; the engine uses Qt Core/Network for local control only.

See CONFIG_SCHEMA.md and CONTROL_PROTOCOL.md for exact implemented behavior.

---

**Target specification:** The UI is a native Windows application written with Qt 6 Qt Quick/QML and C++ state models. It is **not** a web application. The separate C++ tracking/OSC engine remains functional without this program.

## Product experience

The application should feel like a precise hardware/audio control utility: strong hierarchy, readable units, meaningful live status, fast access to frequently used settings, restrained dark/light themes, and sensible defaults. Do not merely reproduce the existing ImGui controls in a new framework. No false visual affordances: controls must alter real engine configuration, or clearly show that they are disabled/not implemented.

### Navigation / screen map

1. **Overview** — connected cameras, per-camera freshness, hand presence, stream/OSC throughput, recent errors, quick start/stop and summary of active profile.
2. **Devices** — discover devices; display serial/alias, firmware/SDK metadata if exposed, enabled state, left/right/Both/None assignment; test/restart one device; explicit device-versus-service states. Optional fixed-rig transforms.
3. **Processing** — visually select camera, hand and channel (palm, wrist, fingers/joints, pinch/grab); show raw versus processed values; parameter groups for smoothing, deadzone, offsets/scale, units and transform. Raw bypass always available. Clearly show whether changes are per-device, per-hand or global.
4. **OSC Output** — destination IP/port, legacy/v2 mode, live sending stats, enable controls, output Hz, packet mode, raw/processed source and output inspector with address, value and sample age. Test-send a harmless diagnostic OSC message; don't use that as proof TouchDesigner is listening.
5. **Profiles & Diagnostics** — save/load/duplicate presets, schema and migration status, searchable bounded logs, structured events, copy/export support bundle and ability to simulate disconnects in a dedicated development mode.

### Main overview layout

```text
Hand Bridge                                    Profile: Installation A [Applied]
Overview | Devices | Processing | OSC Output | Profiles & Diagnostics
-----------------------------------------------------------------------------
Tracking Service: Connected     Engine: Running     OSC: Sending -> 127.0.0.1:9000

Camera A / dev1                    Camera B / dev2
[Connected] [Subscribed]           [Connected] [Subscribed]
Frames: 114 Hz  Last: 8 ms         Frames: 111 Hz  Last: 11 ms
Hands: Left present                Hands: Right present
Drop count: 0                     Drop count: 0

[Preview: simplified skeletons]      [Detailed diagnostics / event history]

Output: 72 Hz   36 OSC packets/sec    Receiver confirmation: Not available
```

The numbers above are illustrative UI copy, not measured performance targets.

## Required interaction rules

- A clearly distinguished **Connected**, **Subscribed**, **Frames arriving**, **Hand visible**, **Processing alive**, **OSC sending** state for each relevant pipeline stage.
- No-hands visible is valid tracking state; do not present it as device failure.
- Every editable setting shows unit, bounded range and default/reset affordance. Dragging a knob should never silently change the parameter's interpretation.
- Provide device-specific and global configuration precedence and a visible indicator of the currently selected scope.
- Use a **command → acknowledgment** control flow. A GUI editor change may be `Unsaved`; applied state only appears after the engine confirms validation. Rejections show an actionable message and retain last good settings.
- UI subscriptions are throttled and bounded; preview data is lossy by design, but engine tracking/OSC data must not depend on preview subscriptions.
- Closing or crashing UI does **not** kill headless host. Reopening reads current engine truth rather than assuming stale local UI values.
- On GUI loss/reconnection, indicate `Connecting`, `Disconnected`, or `Engine not running`, show start/open instructions, and retry without blocking interface.
- All user configuration changes are keyboard-accessible where practical. Avoid tiny controls and subtle contrast.
- Unsafe actions such as `Reset all`, `Clear device aliases` and `Restart tracking service` require confirmation and should make scope explicit. Do not attempt to restart system-wide Ultraleap service without documenting permissions and side effects.
- Optional 3D skeleton viewport must support independent overlays for Camera A and B, oriented axes and position units; do not require Qt 3D renderer to ship basic GUI.

## Processing editor contract

For every supported channel provide:
- Channel identifier, source device/hand, type (scalar, vec3, quaternion/bool), units, valid flag and last update.
- `Enable transmission` separate from `Enable processing` and `Raw/Processed` source selection.
- Filters: time-based smoothing with explicit alpha/time constant, configurable dead zone, scale and offset. Enable per-channel only when applicable.
- Pose orientation handling must be quaternion-safe; avoid treating quaternion components as ordinary independent coordinates if smoothing rotations.
- Visual preview of raw and processed coordinates; distinguish simulated from actual hardware feed.
- Thresholded pinch/grab gestures use hysteresis and explicit enter/exit thresholds to avoid jitter; default raw strengths remain available.
- Processing filters reset appropriately on hand loss, identity change and device restart, avoiding cross-person state contamination.

## Suggested native Qt layering

- `handbridge-control.exe`: Qt Quick/QML views and reusable controls, C++ `QAbstractItemModel`/QObject state models, IPC client.
- Engine local-only QtCore/QtNetwork IPC server; use structured, bounded-length messages and explicit version handshake.
- Do not link `QtQuick`, rendering, or GUI components into engine binary.
- Avoid QML JavaScript for domain rules. Validation, state and commands live in C++ so headless mode behaves identically.

## Functional UI acceptance tests

1. Simulated two cameras shown with distinct serials/aliases and last-frame age.
2. Toggle a channel and validate the corresponding OSC bytes/decoded message output changes.
3. Change processing value; verify both live state acknowledgment and changed output.
4. Change OSC port; validate active socket destination and persisted setting on restart.
5. Close UI during simulated tracking; engine and OSC maintain output.
6. Crash/kill UI; engine and OSC maintain output.
7. Reopen UI; previously applied configuration is displayed accurately.
8. Configure invalid port or malformed transform; engine rejects it without corrupting last good config.
9. Disconnect one mock camera; other camera remains live and its UI status remains correct.
10. Show `UNTESTED ON HARDWARE` for anything validated solely with replay/mock input.
