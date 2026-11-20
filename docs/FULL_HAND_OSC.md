# Full hand skeleton OSC

Hand Bridge 2.0.4 can send the tracked forearm, palm and all 20 digit segments in a single OSC message per hand per emitted frame. Enable **OSC Output → Full hand skeleton → Apply changes**. For startup restore, give the profile a name in **Profiles & Diagnostics** and click **Save applied session**. A schema-2 profile can enable it with `{"schema_version":2,"channels":{"sendFullSkeleton":true}}`.

Listen on the port in **OSC Output** (default UDP 9000). Each selected hand arrives at `/handbridge/v2/{alias}/{left|right}/skeleton`. The same port can also carry the existing `/leap/...` scalar messages. Each skeleton packet carries raw measurements in its camera's coordinate frame. Position, width and pinch distance are millimeters, velocity is mm/s, grab angle is radians, and quaternion order is **W, X, Y, Z**. The output rate, device enable, alias and hand selection apply. The legacy channel switches, smoothing, offset, scale, rotation and meter conversion apply only to the legacy stream.

## Read the packet

The included **handbridge_skeleton.py** maps the arguments to named fields. Load it into a TouchDesigner Text DAT named `handbridge_skeleton`, or import it as a normal Python module in Blender. In the repository it lives under `examples/`; in the Mac distribution it sits beside this guide.

In TouchDesigner, set an **OSC In DAT** to the bridge's UDP port. A single camera can send left and right poses with the same frame ID. Store each camera and side separately so the second hand has its own pose and a loss from one camera cannot clear the other camera's hand. This example keeps the latest event for each camera and side for inspection.

```python
def onReceiveOSC(dat, rowIndex, message, byteData, timeStamp, address, args, peer):
    if address.startswith('/handbridge/v2/'):
        hand = op('handbridge_skeleton').module.decode_packet(byteData)
        if hand is not None:
            dat.store('skeleton_' + hand['alias'] + '_' + hand['side'], hand)
```

For a single person's two hands above `dev1`, read `dat.fetch('skeleton_dev1_left', None)` and `dat.fetch('skeleton_dev1_right', None)`. If two cameras see one hand, select the calibrated alias for that rig control. If multiple people can put hands of the same side over one camera, also distinguish their native hand IDs and expire lost tracks.

In a Python OSC callback that already supplies typed arguments, use `decode_skeleton(address, args)`. A raw UDP socket can pass one datagram to `decode_packet(packet)`. Both functions return `None` for unrelated messages. A loss produces a small dictionary with `valid=False`. A present hand provides named bones as follows. [OSC In DAT reference](https://docs.derivative.ca/OSC_In_DAT)

```python
if hand and hand['valid']:
    wrist_mm = hand['arm']['next'] if hand['arm'] else None
    proximal = hand['digits']['index']['bones']['proximal']
    if proximal:
        mcp_mm = proximal['prev']
        pip_mm = proximal['next']
        rotation_wxyz = proximal['rotation']
```

## Map the T2 rig

| Rig control | Measurement |
|---|---|
| `R_root` | Place at `arm.next` (wrist). |
| `R_forearm` | `arm.prev` is the elbow, `arm.next` the wrist. Orient the fixed-length forearm from elbow toward wrist, keeping the wrist anchored. |
| `R_wrist` | Use calibrated palm rotation, keeping `R_palm` at its bind rotation. The forearm and wrist can rotate independently. |
| `R_index_proximal` | `digits.index.bones.proximal` spans MCP → PIP. Intermediate spans PIP → DIP, distal spans DIP → tip. Apply the same segment names to middle, ring and pinky. |
| `R_thumb_metacarpal` | LeapC supplies a zero-length segment. Keep the model's short control at bind or use calibrated opposition. Drive the three physical links with thumb proximal, intermediate and distal. |

For the left hand use the `L_` bones. Map each camera once into the scene. The usual level-tabletop example sends raw `(x,y,z)` to Blender meters as `0.001 * (x,-z,y)`. Fit the actual mount and take an open neutral pose per hand. Convert the tracker orientation into armature space, apply the bind-pose correction from your rig map, then compute each bone's rotation relative to its posed parent. Keep the model's link lengths fixed. If a segment has usable endpoints and a zero quaternion, derive its direction from `next - prev` and use its calibrated hinge axis for roll. [Ultraleap coordinate system and bones](https://docs.ultraleap.com/api-reference/tracking-api/leapc-guide/leap-concepts.html)

## Wire layout

One datagram contains one OSC message using the `i` (int32), `h` (int64), and `f` (float32) tags. All numeric arguments are big-endian. Every live sample has **278 arguments**. `dev1/right` uses 1,444 bytes. The largest configured alias fits in a 1,472-byte UDP payload. Each hand sample is self-contained.

| Zero-based arguments | Values, in order |
|---|---|
| 0–3 | Present (`i`, 1 or 0), tracking frame ID (`h`), source timestamp in µs (`h`), device-local native hand ID (`h`, −1 when unavailable). Loss ends after these four arguments. |
| 4–10 | Visible time in µs (`h`), SDK hand flags (`i`, 32-bit bitmask), confidence, pinch distance, grab angle, pinch strength, grab strength (five `f`). Hyperion 6.2 reports confidence as a reserved constant. |
| 11–30 | Palm position XYZ, stabilized position XYZ, velocity XYZ, normal XYZ, direction XYZ, rotation WXYZ, width (20 floats). Stabilized position is the SDK value. |
| 31–42 | Forearm geometry valid (`i`), elbow XYZ, wrist XYZ, forearm rotation WXYZ, width. |
| 43–277 | Five 47-argument blocks in **thumb, index, middle, ring, pinky** order. |

Each digit begins with its native finger ID, extension flag (0/1) and a four-bit geometry-validity mask (three int32s). Next are **metacarpal, proximal, intermediate, distal**, each with 11 floats: previous joint XYZ, next joint XYZ, rotation WXYZ, and width. Bit 0 corresponds to metacarpal, bit 3 to distal. An unavailable digit has ID −1. Invalid geometry has a cleared bit and a zero-filled slot. A valid segment can have zero length. Unavailable rotation and width use zero values. The decoder represents invalid arm/bones as Python `None`. The SDK flags are preserved as a bit field; the decoder returns an unsigned value.

Group tracks by **alias, side and hand ID**. For comparisons within one tracking session use the source timestamp and frame ID. A disappearing hand sends a single loss with its former ID. Disappearance in a camera frame uses that frame's metadata. Stale frames, routing changes, device/service loss and orderly shutdown use the last observed frame metadata, so a loss can share the last live sample's frame ID. Give that loss priority for the same track and frame. A receiver should also expire silent tracks, since UDP may drop the loss packet. Restart velocity history on reacquisition or a new ID. Estimate per-bone velocities from successive samples of the same track and a positive source-time delta. Palm velocity is available directly.
