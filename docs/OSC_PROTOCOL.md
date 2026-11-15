# OSC contract for TouchDesigner

**State:** Legacy behavior observed statically in the original repository; verify by running the original program or fixtures in Phase 0. All improvements MUST retain an opt-in-free legacy compatibility mode.

## Transport
- OSC/UDP; configured address normally `127.0.0.1` to communicate with TouchDesigner on the same PC.
- Existing `src/core/ConfigManager.cpp` loads the `osc_port` setting and falls back to **9000** if missing. Earlier human-facing examples may show 7000. **Audit before finalizing the user default.** Never silently change a deployed setting.
- Legacy output currently sends separate OSC messages containing single `float` arguments. `src/pipeline/04_OscSender.cpp::sendBundle` is a stub; it must not be represented as an implemented feature.
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

## Optional v2 (NOT CURRENTLY IMPLEMENTED)

After compatibility is stable, an opt-in frame-oriented protocol may reduce packet overhead. Proposed examples below are DESIGN EXAMPLES only:

```text
/handbridge/v2/device/dev1/hand/left/valid               int32(1 or 0)
/handbridge/v2/device/dev1/hand/left/palm/position       float32(x,y,z)
/handbridge/v2/device/dev1/hand/left/index/tip_position float32(x,y,z)
/handbridge/v2/device/dev1/status/frames_fresh          int32(1 or 0)
```

For v2 define axis convention, units (e.g. meters), hand IDs, validity, sequence and timestamps, bundle framing, maximum packet size, data loss policy and schema version. Never mix v1 scalar fields and v2 vector arguments at the same OSC address. If large OSC bundles approach UDP fragmentation thresholds, split by device/hand/frame with documentation; do not assume unlimited datagram size.

## TouchDesigner connection contract
- An **OSC In CHOP** can receive scalar numeric channels on the same port; inspect path mapping options and device names. An **OSC In DAT** is useful to inspect message paths and bundles or debug timestamps.
- Sample test must demonstrate dev1 and dev2 arriving at the same local receiver port with stable per-device addresses, and disconnect/invalid state behavior.
- If TouchDesigner and the bridge are on the same PC, use `127.0.0.1` and the exact configured port, not the PC's LAN IP, unless the user chooses to route remotely.
