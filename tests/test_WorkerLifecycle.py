"""Native host startup failure and natural worker shutdown; no SDK/GUI required."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import uuid

from test_HeadlessUdp import decode


def environment(root):
    """Redirect only child-process state; preserve the installed Qt loader paths."""
    result = os.environ.copy()
    for variable in ("HANDBRIDGE_LEAPC_LIBRARY", "LEAP_SDK_ROOT"):
        result.pop(variable, None)
    for variable, leaf in {
        "CFFIXED_USER_HOME": "home",
        "XDG_CONFIG_HOME": "config",
        "XDG_DATA_HOME": "data",
        "XDG_CACHE_HOME": "cache",
        "XDG_RUNTIME_DIR": "runtime",
        "APPDATA": "config",
        "LOCALAPPDATA": "data",
    }.items():
        path = root / leaf
        path.mkdir(mode=0o700, exist_ok=True)
        result[variable] = str(path)
    return result


def arguments(root, name):
    state = root / name
    state.mkdir(mode=0o700)
    # Keep Darwin's Unix-domain socket path short; this is an isolated mock test,
    # not acceptance of the production endpoint's same-user protection.
    endpoint = "hb-w-" + uuid.uuid4().hex[:12]
    return ["--headless", "--endpoint", endpoint, "--state-dir", str(state)]


def startup_failure(executable, root, child_environment):
    missing = root / "missing-LeapC.dylib"
    command = [executable] + arguments(root, "startup") + ["--leapc-library", str(missing)]
    # subprocess.run kills only if this deadline fails. Never turn a killed or
    # aborted worker into an accepted startup-error result.
    result = subprocess.run(command, cwd=root, env=child_environment,
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
    assert "handbridge-engine:" in result.stderr, result.stderr


def natural_shutdown(executable, root, child_environment):
    packets, reader_errors = [], []
    done = threading.Event()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(0.05)

        def listen():
            while not done.is_set():
                try:
                    packets.append(decode(receiver.recv(8192)))
                except socket.timeout:
                    continue
                except Exception as error:
                    reader_errors.append(error)
                    return

        command = [executable] + arguments(root, "natural") + [
            "--simulate", "--duration-ms", "150", "--host", "127.0.0.1",
            "--port", str(receiver.getsockname()[1]),
        ]
        listener = threading.Thread(target=listen)
        listener.start()
        try:
            # Successful execution must finish through the host's own timer,
            # stop request, wake and join. No SIGTERM/kill success fallback.
            result = subprocess.run(command, cwd=root, env=child_environment,
                                    capture_output=True, text=True, timeout=10)
        finally:
            done.set()
            listener.join(timeout=2)
        assert not listener.is_alive(), "UDP receiver failed to stop"
    assert not reader_errors, reader_errors
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    summary = json.loads(result.stdout.strip())
    assert summary["source"] == "simulated" and summary["simulated"] is True, summary
    assert summary["hardware_verified"] is False, summary
    assert set(summary["devices"]) == {"SIM-A", "SIM-B"}, summary
    assert all(device["frames_processed"] > 0 for device in summary["devices"].values()), summary
    assert summary["output"]["errors"] == 0, summary
    assert summary["output"]["attempts"] > 0, summary
    assert summary["output"]["successes"] > 0, summary
    assert any(address.startswith("/leap/dev1/") and value != 0 for address, value in packets), packets
    assert any(address.startswith("/leap/dev2/") and value != 0 for address, value in packets), packets


def main(executable):
    executable = str(Path(executable).resolve(strict=True))
    # /tmp avoids lengthy /var/folders prefixes on hosted Darwin. Windows uses
    # its normal temporary directory and named-pipe endpoint implementation.
    temporary_parent = "/tmp" if os.name == "posix" else None
    with tempfile.TemporaryDirectory(prefix="hb-w-", dir=temporary_parent) as directory:
        root = Path(directory)
        child_environment = environment(root)
        startup_failure(executable, root, child_environment)
        natural_shutdown(executable, root, child_environment)
    print("PASS: worker startup error exits 1, natural timed stop exits 0, final snapshot and decoded mock UDP observed")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_WorkerLifecycle.py /absolute/path/to/handbridge-engine")
    main(sys.argv[1])
