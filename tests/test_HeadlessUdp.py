"""Real process + loopback UDP checks; Python is a test receiver, never the engine."""
import json
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "examples"))
from handbridge_skeleton import decode_packet


def decode(packet):
    offset = 0

    def string():
        nonlocal offset
        end = packet.index(b"\0", offset)
        value = packet[offset:end].decode("ascii")
        padded = (end + 4) & ~3
        assert packet[end:padded] == b"\0" * (padded - end)
        offset = padded
        return value

    address, tags = string(), string()
    assert address.startswith("/leap/") and tags == ",f"
    assert len(packet) == offset + 4
    return address, struct.unpack(">f", packet[offset:])[0]


def run_receiver(engine, scenario, config=None, decoder=decode):
    packets, errors = [], []
    done = threading.Event()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(0.05)

        def listen():
            while not done.is_set():
                try:
                    packets.append(decoder(receiver.recv(8192)))
                except socket.timeout:
                    continue
                except Exception as error:
                    errors.append(error)
                    break

        thread = threading.Thread(target=listen)
        thread.start()
        command = [*engine, "--simulate", "--headless", "--host", "127.0.0.1",
                   "--port", str(receiver.getsockname()[1]), "--duration-ms", "1600",
                   "--scenario", scenario]
        if config:
            command += ["--config", str(config)]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=15)
        finally:
            # Drain queued shutdown loss datagrams before stopping the receiver.
            done.wait(0.15)
            done.set()
            thread.join(timeout=2)
        assert not errors, errors
        assert result.returncode == 0, (result.stdout, result.stderr)
        summary = json.loads(result.stdout.strip().splitlines()[-1])
        assert summary["source"] == "simulated" and summary["hardware_verified"] is False
        assert summary["output"]["errors"] == 0
        assert summary["devices"]["SIM-B"]["frames_processed"] > 10
        return packets, summary


def check_engine(engine):
    packets, summary = run_receiver(engine, "stall")
    a = "/leap/dev1/left/palm/tx"
    b = "/leap/dev2/right/palm/tx"
    first_loss = next(index for index, item in enumerate(packets) if item == (a, 0.0))
    resumed = next(index for index in range(first_loss + 1, len(packets))
                   if packets[index][0] == a and packets[index][1] > 0)
    assert any(address == b and value > 0 for address, value in packets[first_loss:resumed])
    assert any(address == a and value > 0 for address, value in packets[:first_loss])
    assert summary["devices"]["SIM-A"]["reconnect_attempts"] >= 1
    assert summary["devices"]["SIM-A"]["frames_fresh"] is True
    assert summary["devices"]["SIM-B"]["reconnect_attempts"] == 0

    with tempfile.TemporaryDirectory(prefix="handbridge-process-") as directory:
        config = Path(directory) / "legacy.json"
        config.write_text(json.dumps({"osc_port": 9000,
            "device_aliases": {"SIM-A": "dev7", "SIM-B": "dev8"},
            "hand_assignments": {"SIM-A": "LEFT", "SIM-B": "RIGHT"},
            "booleanSettings": {"sendPalm": True, "sendWrist": False,
                "sendThumb": False, "sendIndex": False, "sendMiddle": False,
                "sendRing": False, "sendPinky": False,
                "sendPinchStrength": False, "sendGrabStrength": False}}), encoding="utf-8")
        before = config.read_bytes()
        selected, _ = run_receiver(engine, "normal", config)
        assert selected and all(address.startswith(("/leap/dev7/left/palm/", "/leap/dev8/right/palm/"))
                                for address, _ in selected)
        assert config.read_bytes() == before, "Engine must not silently overwrite config"
        config.write_text(json.dumps({
            "schema_version": 2,
            "channels": {"sendFullSkeleton": True},
            "processing": {"bypass": False, "output_units": "m"},
        }), encoding="utf-8")
        def capture(packet):
            if packet.startswith(b"/leap/"):
                return decode(packet)
            assert len(packet) <= 1472
            sample = decode_packet(packet)
            assert sample is not None
            return sample
        received, _ = run_receiver(engine, "stall", config, capture)
        assert any(isinstance(item, tuple) and item == ("/leap/dev2/right/wrist/ty", struct.unpack(">f", struct.pack(">f", .15))[0])
                   for item in received), "Legacy positions must still follow selected units"
        skeletons = [item for item in received if isinstance(item, dict)]
        assert {(s["alias"], s["side"], s["hand_id"]) for s in skeletons} == {
            ("dev1", "left", 100), ("dev2", "right", 101)}
        for sample in skeletons:
            if not sample["valid"]:
                continue
            assert sample["timestamp_us"] >= 0 and sample["frame_id"] > 0
            assert sample["arm"]["prev"] == (80, 150, 230)
            assert sample["arm"]["next"] == (80, 150, -40)
            assert sample["palm"]["rotation"] == (1, 0, 0, 0)
            assert sample["palm"]["width"] == 80 and sample["pinch_distance"] == 35
            for index, digit in enumerate(sample["digits"].values()):
                assert digit["id"] == index
                bones = list(digit["bones"].values())
                assert all(bone is not None for bone in bones)
                assert all(a["next"] == b["prev"] for a, b in zip(bones, bones[1:]))
                assert bones[-1]["next"] == (index * 10, 250, -20)
            thumb = sample["digits"]["thumb"]["bones"]["metacarpal"]
            assert thumb["prev"] == thumb["next"] and thumb["width"] == 0
        def transitions(alias):
            from itertools import groupby
            return [(valid, len(list(group))) for valid, group in groupby(
                (s for s in skeletons if s["alias"] == alias), key=lambda s: s["valid"])]
        left, right = transitions("dev1"), transitions("dev2")
        assert [valid for valid, count in left] == [True, False, True, False], left
        assert [valid for valid, count in right] == [True, False], right
        assert all(count == 1 for valid, count in left + right if not valid)
        config.write_text('{"schema_version":999,"osc":{"port":7000}}', encoding="utf-8")
        rejected = subprocess.run([*engine, "--simulate", "--config", str(config)],
                                  capture_output=True, text=True, timeout=5)
        assert rejected.returncode != 0, "Unknown future schema must not silently fall back"

    missing_library = str((Path(directory) / "missing-LeapC.dll").resolve())
    for arguments in (["--leapc-library", missing_library], ["--simulate", "--port", "0"], ["--simulate", "--host", "bad-host"],
                      ["--simulate", "--unknown"], ["--simulate", "--duration-ms", "-1"]):
        result = subprocess.run(engine + arguments, capture_output=True, text=True, timeout=5)
        assert result.returncode != 0, arguments
    print("PASS: headless process, isolated stall recovery, decoded UDP, config routing, CLI rejection")


if __name__ == "__main__":
    executable = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="handbridge-udp-test-") as state:
        engine = [executable]
        help_text = subprocess.check_output([executable, "--help"], text=True, timeout=5)
        if "--endpoint" in help_text:
            engine += ["--endpoint", "hb-udp-" + uuid.uuid4().hex[:12], "--state-dir", state]
        check_engine(engine)
