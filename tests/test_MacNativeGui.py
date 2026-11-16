"""Native arm64 Cocoa GUI smoke with an independent simulated engine and UDP."""
import contextlib
import json
import os
from pathlib import Path
import platform
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import zlib

from test_HeadlessUdp import decode
from test_WorkerLifecycle import arguments, environment


class Receiver:
    def __init__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(0.05)
        self.port = self.socket.getsockname()[1]
        self.done = threading.Event()
        self.changed = threading.Condition()
        self.counts = {"dev1": 0, "dev2": 0}
        self.error = None
        self.thread = threading.Thread(target=self.listen)

    def listen(self):
        while not self.done.is_set():
            try:
                address, value = decode(self.socket.recv(8192))
                with self.changed:
                    for alias in self.counts:
                        if address.startswith("/leap/" + alias + "/") and value != 0:
                            self.counts[alias] += 1
                    self.changed.notify_all()
            except socket.timeout:
                continue
            except Exception as error:
                with self.changed:
                    self.error = repr(error)
                    self.changed.notify_all()
                return

    def __enter__(self):
        try:
            self.thread.start()
        except BaseException:
            self.socket.close()
            raise
        return self

    def snapshot(self):
        with self.changed:
            return dict(self.counts)

    def progress(self, before, engine, deadline, label):
        with self.changed:
            while not all(self.counts[alias] > count for alias, count in before.items()):
                if self.error:
                    raise AssertionError("UDP decoding failed: " + self.error)
                if engine.poll() is not None:
                    raise AssertionError(label + ": independent engine exited " + str(engine.returncode))
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(label + ": no new decoded UDP for both simulated devices")
                self.changed.wait(min(0.05, remaining))
            if self.error:
                raise AssertionError("UDP decoding failed: " + self.error)
            if engine.poll() is not None:
                raise AssertionError(label + ": independent engine exited " + str(engine.returncode))
            return dict(self.counts)

    def __exit__(self, *unused):
        self.done.set()
        self.thread.join(timeout=2)
        self.socket.close()
        if self.thread.is_alive():
            raise RuntimeError("UDP receiver thread failed to stop")


def png_dimensions(path):
    """Return dimensions only once Qt has completed a valid PNG through IEND."""
    if not path.exists():
        return None
    if path.stat().st_size > 64 * 1024 * 1024:
        raise AssertionError("Unexpectedly large PNG: " + str(path))
    data = path.read_bytes()
    if len(data) < 8:
        return None
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "Invalid PNG signature: " + str(path)
    offset, dimensions = 8, None
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        end = offset + 12 + size
        if end > len(data):
            return None
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        expected = struct.unpack_from(">I", data, offset + 8 + size)[0]
        assert zlib.crc32(kind + payload) & 0xffffffff == expected, "Invalid PNG CRC"
        if kind == b"IHDR":
            assert size == 13, "Invalid PNG IHDR length"
            dimensions = struct.unpack_from(">II", payload)
        if kind == b"IEND":
            assert size == 0 and end == len(data), "Invalid PNG ending"
            assert dimensions and dimensions[0] >= 1100 and dimensions[1] >= 700, dimensions
            return dimensions
        offset = end
    return None


def wait_capture(path, gui, deadline):
    while time.monotonic() < deadline:
        dimensions = png_dimensions(path)
        if dimensions:
            return dimensions
        if gui.poll() is not None:
            raise AssertionError("GUI exited before completing native capture: " + str(gui.returncode))
        time.sleep(0.03)
    raise TimeoutError("Native Cocoa PNG capture timed out: " + str(path))


def wait_exit(process, deadline, label):
    while process.poll() is None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(label + " failed to exit naturally before the deadline")
        time.sleep(min(0.03, remaining))
    return process.returncode


def main(engine_path, control_path):
    if sys.platform != "darwin" or platform.machine().lower() != "arm64":
        raise RuntimeError("This smoke requires native Darwin arm64; Intel/Rosetta are outside its scope")
    engine_path = str(Path(engine_path).resolve(strict=True))
    control_path = str(Path(control_path).resolve(strict=True))
    base = Path(os.environ.get("HANDBRIDGE_GUI_ARTIFACT_DIR", "work/mac-native-gui")).resolve()
    artifacts = base / uuid.uuid4().hex[:12]
    artifacts.mkdir(parents=True)
    result = {"passed": False, "platform": sys.platform, "architecture": platform.machine(),
              "qt_platform": "cocoa", "qt_quick_backend": "software", "simulated": True,
              "hardware_verified": False, "cycles": [], "forced_failure_cleanup": []}
    children = []
    started = time.monotonic()
    deadline = started + 30
    print("Native GUI artifacts:", artifacts, flush=True)
    try:
        with tempfile.TemporaryDirectory(prefix="hb-g-", dir="/tmp") as temporary, contextlib.ExitStack() as files:
            root = Path(temporary)
            child_environment = environment(root)
            gui_environment = dict(child_environment, QT_QPA_PLATFORM="cocoa", QT_QUICK_BACKEND="software")

            def launch(program, options, label, child_env):
                output = files.enter_context((artifacts / (label + ".stdout.txt")).open("wb"))
                errors = files.enter_context((artifacts / (label + ".stderr.txt")).open("wb"))
                process = subprocess.Popen([program] + options, cwd=root, env=child_env,
                                           stdout=output, stderr=errors)
                children.append((label, process))
                return process

            with Receiver() as receiver:
                engine_options = arguments(root, "engine")
                endpoint = engine_options[engine_options.index("--endpoint") + 1]
                engine_options += ["--simulate", "--duration-ms", "22000", "--host", "127.0.0.1",
                                   "--port", str(receiver.port)]
                engine = launch(engine_path, engine_options, "engine", child_environment)
                result["engine_pid"] = engine.pid
                result["initial_udp"] = receiver.progress({"dev1": 0, "dev2": 0}, engine,
                                                          min(deadline, time.monotonic() + 5), "engine startup")

                for index, label in enumerate(("close", "kill", "reopen"), start=1):
                    capture = artifacts / (str(index) + "-" + label + ".png")
                    options = ["--endpoint", endpoint, "--screenshot", str(capture)]
                    if label != "kill":
                        options += ["--exit-after-ms", "2500"]
                    gui = launch(control_path, options, label, gui_environment)
                    dimensions = wait_capture(capture, gui, min(deadline, time.monotonic() + 6))
                    assert engine.poll() is None, "GUI launch/render interrupted the engine"
                    if label == "kill":
                        assert gui.poll() is None, "Controller exited before intentional kill"
                        gui.kill()
                        code = wait_exit(gui, min(deadline, time.monotonic() + 2), "killed controller")
                        assert code == -signal.SIGKILL, (label, code)
                    else:
                        code = wait_exit(gui, min(deadline, time.monotonic() + 4), "controller " + label)
                        assert code == 0, (label, code)
                    assert engine.poll() is None, "GUI close/kill stopped the independent engine"
                    # Begin counting after the GUI has exited: earlier packets
                    # cannot satisfy the continuing-output assertion.
                    before = receiver.snapshot()
                    after = receiver.progress(before, engine, min(deadline, time.monotonic() + 2),
                                              "after GUI " + label)
                    result["cycles"].append({"action": label, "gui_pid": gui.pid,
                                             "gui_exit_code": code, "png": capture.name,
                                             "dimensions": dimensions, "udp_before": before,
                                             "udp_after": after, "engine_pid": engine.pid,
                                             "engine_alive": engine.poll() is None})
                    print("PASS: native Cocoa", label, "PNG", dimensions,
                          "engine PID", engine.pid, "fresh simulated UDP", after, flush=True)

                code = wait_exit(engine, deadline, "timed simulated engine")
                assert code == 0, ("engine", code)
                summary = json.loads((artifacts / "engine.stdout.txt").read_text())
                assert summary["simulated"] is True and summary["hardware_verified"] is False, summary
                assert summary["source"] == "simulated" and summary["output"]["errors"] == 0, summary
                assert set(summary["devices"]) == {"SIM-A", "SIM-B"}, summary
                assert all(device["frames_processed"] > 0 for device in summary["devices"].values()), summary
                result["engine_exit_code"] = code
                result["engine_summary"] = summary
            result["passed"] = True
    except BaseException as error:
        result["error"] = repr(error)
        raise
    finally:
        for label, process in children:
            if process.poll() is None:
                # Only a failed test/deadline reaches this cleanup. Intentional
                # GUI kill is recorded separately as a required lifecycle case.
                result["forced_failure_cleanup"].append(label)
                process.kill()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    result.setdefault("cleanup_errors", []).append(label + " did not exit after SIGKILL")
        result["elapsed_seconds"] = time.monotonic() - started
        (artifacts / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS: native arm64 Cocoa capture, GUI close/kill/reopen and continuing mock UDP; hardware NOT RUN")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_MacNativeGui.py /absolute/engine /absolute/control")
    main(sys.argv[1], sys.argv[2])
