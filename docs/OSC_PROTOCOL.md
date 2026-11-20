# OSC contract for TouchDesigner

**State:** Source-audited legacy contract, frozen by decoded UDP golden fixtures in this increment. Physical TouchDesigner validation remains UNTESTED ON HARDWARE.

## Transport
- OSC/UDP; configured address normally `127.0.0.1` to communicate with TouchDesigner on the same PC.
- Existing `src/core/ConfigManager.cpp` loads the `osc_port` setting and falls back to **9000** if missing. The original README example shows 7000; no deployed user config is tracked. The source-backed default remains 9000 and loaded valid ports are preserved.
- Legacy output sends separate OSC messages containing single float32 arguments (OSC type tag `,f`, network byte order). `src/pipeline/04_OscSender.cpp::sendBundle` is a stub; it must not be represented as an implemented feature.
- OSC UDP packet delivery is not guaranteed; a successful sender call is not proof of TouchDesigner receipt.

## Source-confirmed legacy address family (examples)

The original `src/pipeline/03_DataProcessor.cpp` constructs `"/leap/" + alias + "/" + handType + "/" + dataType + optionalSubType` and sends a scalar float per OSC address. Examples for an alias `dev1` and lower-case handedness `left`:

| Address | Argument (legacy) | Meaning |
|---|---|---|
| `/leap/dev1/left/palm/tx` | float | Palm X, millimeters |
| `/leap/dev1/left/palm/ty` | float | Palm Y, millimeters |
| `/leap/dev1/left/palm/tz` | float | Palm Z, millimeters |
| `/leap/dev1/left/wrist/tx` | float | Wrist X if valid |
| `/leap/dev1/left/finger/index/tx` | float | Index fingertip X |
| `/leap/dev1/left/finger/index/ty` | float | Index fingertip Y |
| `/leap/dev1/left/finger/index/tz` | float | Index fingertip Z |
| `/leap/dev1/left/pinchStrength` | float | Native pinch strength |
| `/leap/dev1/left/grabStrength` | float | Native grab strength |
| `/leap/dev1/left/palm/orientation/qw` | float | Palm orientation W if enabled |
| `/leap/dev1/left/palm/velocity/vx` | float | Palm velocity X if enabled |
| `/leap/dev1/left/palm/normal/nx` | float | Palm normal X if enabled |
| `/leap/dev1/left/visibleTime` | float | Time visible (source converts microseconds to seconds) |
| `/leap/dev1/left/finger/index/isExtended` | float | Extension 0/1 when enabled |

The same families exist for other fingers, `right` and other aliases. The exact full address set and absent-hand output must be captured in golden tests before refactoring. The source currently sends zeros after a hand disappears in a subsequent frame, but SDK loss and device disconnection are separate edge cases. Do not assume a direct zero-value means tracking invalid; zero may be a real coordinate.

## Golden compatibility testing required
- Snapshot all actual emitted legacy addresses for a representative input frame, including exact case, argument count/type, units, order where meaningful, and selected channel filters.
- Test no hands, hand disappearing, device disappearing, UI edits, save/reload of alias/hand assignments and two-device streams.
- Add a tiny test OSC receiver or packet decoder in C++ tests and an optional TouchDesigner receiving example. Assertions should decode OSC packets, not compare UDP payload strings.
- Preserve default/legacy behavior until an explicit user-selected protocol/profile migration.

## Full hand skeleton in 2.0.4

Enable **OSC Output → Full hand skeleton → Apply changes** for one complete raw hand per UDP datagram. The address is `/handbridge/v2/{alias}/{side}/skeleton`. The first four arguments carry presence, tracking frame ID, capture time and device-local hand ID. The pose carries forearm endpoints, palm and hand data, and four named bone segments for each of five digits. [Wire layout, decoder and T2 rig mapping](FULL_HAND_OSC.md)

## TouchDesigner connection contract
- An **OSC In CHOP** can receive scalar numeric channels on the same port; inspect path mapping options and device names. An **OSC In DAT** is useful to inspect message paths and bundles or debug timestamps.
- Sample test must demonstrate dev1 and dev2 arriving at the same local receiver port with stable per-device addresses, and disconnect/invalid state behavior.
- If TouchDesigner and the bridge are on the same PC, use `127.0.0.1` and the exact configured port, not the PC's LAN IP, unless the user chooses to route remotely.

## Exact compatibility fixtures and loss boundaries (implemented)

[Golden fixtures](../tests/fixtures/osc/) and
[test_LegacyOscContract.cpp](../tests/test_LegacyOscContract.cpp) exercise the
preserved production processor and serializer through a real bound UDP receiver
with an independent parser. [Full audit](reports/OSC_AUDIT.md) gives baseline lines.
A full valid hand sends 23 default packets or 39 with all optional channels.
Default loss sends 28 zeros once; all-options loss sends 34. Finger `exists=0`
is emitted only on loss, never `exists=1`. Optional orientation/velocity/normal
are not cleared on loss; extension loss depends on fingertip position selection.
These quirks are preserved, so zero is not a general explicit validity signal.

The new headless supervisor submits an empty frame once on stale/device/service
loss and shutdown. This repairs indefinite held default pose in that host while
retaining the scalar loss contract. Optional orientation/velocity/normal may
still hold their last value at receivers; consult engine freshness state. The
original hardware path has not gained the new supervisor yet.

The legacy fixtures define packet order. Each scalar occupies one datagram. The optional v2 stream carries its own capture timestamp. Invalid wrist/finger geometry
is omitted, and top-level hand validity is not interpreted by the legacy processor.
Velocity remains mm/s, normal dimensionless, quaternion order WXYZ, visibleTime
seconds from source microseconds. Multiple DTO values retain historical separate
packets rather than changing argument arity.

Sender readiness, attempted packets, actual checked OS-send successes, bytes,
errors and last attempt/success timestamps are distinct. Even OS acceptance does
not establish receipt. Sender errors are rate-limited in logs. The transport uses
oscpack serialization with a checked native UDP socket because upstream oscpack's
Send ignores native return values. Destination changes recreate the socket;
automatic sender reset/backoff remains future work, not a claimed feature.

## Native controller processing and settings (implemented)

The Qt controller uses configuration schema2, while OSC remains legacy scalar
messages. Default raw bypass, millimeters, address case, 9000 fallback and goldens
are unchanged. Channel toggles, per-device mute/filter/alias, global send enable,
destination and rate are engine settings, applied only after validation/ack.
Routing changes clear the old enabled pose addresses before changing aliases or
muting. Loss messages bypass output rate limiting. A successful OS send is still
not receipt; use TouchDesigner to verify the receiver.

Turning raw bypass off explicitly enables time-based positional smoothing,
deadzone and scale→Euler XYZ rotation→offset→unit conversion. Velocity gets no
position offset; normal/orientation rotate consistently. Units may explicitly be
changed to meters; adjust TouchDesigner scaling for that selected profile. These
are opt-in value changes at the same legacy addresses, not a new OSC protocol.

Optional gestures_enabled adds two scalar float0/1 addresses per selected hand:
`/leap/{alias}/{hand}/pinchActive` and `grabActive`. Enter/exit thresholds use
hysteresis; loss resets once. Native pinchStrength/grabStrength are preserved.
No smoothing/gesture motion is synthesized after tracking disappears. This
controller applies processing globally to legacy values. The optical skeleton uses raw camera space. [Settings and bounds](CONFIG_SCHEMA.md)

Controller shutdown/crash leaves the QtCore/Network engine and OSC worker alive.
Raw/processed point previews and measured telemetry travel over sampled local IPC;
the GUI never carries the tracking stream used to send packets. See CONTROL_PROTOCOL.md.

## Hyperion 6 source compatibility

The optional real adapter uses the official Hyperion 6.2 SDK, preserving raw palm /
arm / digit/bone observations, millimeters, quaternion order, native strengths and
source timestamps before the existing processing/OSC stages. It does not introduce
a new address format or expose native IDs as new legacy channels. Known native
hand-ID changes reset only that hand's filter/gesture state; raw bypass and legacy
golden packets remain unchanged. No-hands, stale/device/service loss still use the
documented legacy loss rules and retain their optional-channel limitations.

The SDK/client version checks and a successful UDP send do not prove that actual
TouchDesigner received camera data. Complete the physical receiver checklist with
Hyperion 6.2 or newer 6.x and record real results separately.

## Darwin send errors and SIGPIPE

The Darwin sender enables SO_NOSIGPIPE per socket before connect/send. It keeps
the process signal policy intact and routes socket-option/send errors through the
existing readiness and send-error counters. Legacy address/type/units/one-scalar
packet behavior and Windows/Linux transport paths are unchanged. The broadcast
error regression and an isolated default/unblocked-SIGPIPE test remain enabled on
Darwin; no failed case is hidden by a global ignored signal or a test skip.
Actual arm64 verification and any infrastructure blocker: reports/MACOS_ARM64.md.
