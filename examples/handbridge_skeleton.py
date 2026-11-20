"""Decode Hand Bridge 2.0.4 skeletons from OSC callback arguments or UDP bytes.

Positions and widths are camera-space millimeters. Rotations are WXYZ.
The module uses the Python standard library and can be loaded into a Text DAT.
"""

import struct


DIGITS = ("thumb", "index", "middle", "ring", "pinky")
SEGMENTS = ("metacarpal", "proximal", "intermediate", "distal")
HEADER_TAGS = "ihhh"
POSE_TAGS = HEADER_TAGS + "hi" + "f" * 25 + "i" + "f" * 11 + ("iii" + "f" * 44) * 5


def _segment(values):
    return {
        "prev": tuple(values[:3]), "next": tuple(values[3:6]),
        "rotation": tuple(values[6:10]), "width": values[10],
    }


def decode_skeleton(address, args):
    """Return a named hand sample or loss event; ignore unrelated OSC addresses."""
    path = address.split("/")
    if len(path) != 6 or path[:3] != ["", "handbridge", "v2"] or path[5] != "skeleton":
        return None
    if path[4] not in ("left", "right") or not args:
        raise ValueError("Invalid skeleton address or empty sample")
    if args[0] not in (0, 1) or len(args) != (len(POSE_TAGS) if args[0] else len(HEADER_TAGS)):
        raise ValueError("Invalid skeleton presence or argument count")
    sample = {
        "alias": path[3], "side": path[4], "valid": bool(args[0]),
        "frame_id": args[1], "timestamp_us": args[2], "hand_id": args[3],
    }
    if not sample["valid"]:
        return sample
    sample.update(
        visible_time_us=args[4], flags=args[5] & 0xffffffff, confidence=args[6],
        pinch_distance=args[7], grab_angle=args[8], pinch_strength=args[9], grab_strength=args[10],
        palm={
            "position": tuple(args[11:14]), "stabilized_position": tuple(args[14:17]),
            "velocity": tuple(args[17:20]), "normal": tuple(args[20:23]),
            "direction": tuple(args[23:26]), "rotation": tuple(args[26:30]), "width": args[30],
        },
        arm=_segment(args[32:43]) if args[31] else None,
        digits={},
    )
    for index, digit in enumerate(DIGITS):
        start = 43 + index * 47
        finger_id, extended, mask = args[start:start + 3]
        bones = {}
        for bit, name in enumerate(SEGMENTS):
            offset = start + 3 + bit * 11
            bones[name] = _segment(args[offset:offset + 11]) if mask & (1 << bit) else None
        sample["digits"][digit] = {"id": finger_id, "extended": bool(extended), "bones": bones}
    return sample


def decode_packet(packet):
    """Decode one UDP datagram. Returns None for legacy and other OSC messages."""
    if not packet.startswith(b"/handbridge/v2/"):
        return None
    address_end = packet.index(b"\0")
    address = packet[:address_end].decode("ascii")
    if not address.endswith("/skeleton"):
        return None
    tags_start = (address_end + 4) & ~3
    tags_end = packet.index(b"\0", tags_start)
    tags = packet[tags_start:tags_end].decode("ascii")
    if tags not in ("," + HEADER_TAGS, "," + POSE_TAGS):
        raise ValueError("Unexpected skeleton OSC type tags")
    data_start = (tags_end + 4) & ~3
    fmt = ">" + tags[1:].replace("h", "q")
    if len(packet) - data_start != struct.calcsize(fmt):
        raise ValueError("Incorrect skeleton payload size")
    return decode_skeleton(address, struct.unpack_from(fmt, packet, data_start))
