"""Check the Mac controller's actual launch modes without accessing camera hardware."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

from test_WorkerLifecycle import environment


def main(control_path):
    with tempfile.TemporaryDirectory(prefix="hb-start-") as directory:
        root = Path(directory)
        control = root / "handbridge-control"
        shutil.copy2(control_path, control)
        engine = root / "handbridge-engine"
        engine.write_text(
            "#!/bin/sh\n"
            'printf "%s\\n" "$*" >> "$HB_STARTUP_TRACE"\n'
            'if [ "$1" = "--check-leapc" ]; then\n'
            '  printf \'%s\\n\' \'{"provider":"leapc"}\'\n'
            'fi\n'
        )
        engine.chmod(0o700)
        trace = root / "launches"
        child_env = dict(environment(root), QT_QPA_PLATFORM="offscreen",
                         QT_QUICK_BACKEND="software", HB_STARTUP_TRACE=str(trace))
        endpoint = "hb-start-" + uuid.uuid4().hex[:12]
        hardware = ["--check-leapc", "--endpoint " + endpoint]
        for options, expected in (([], hardware), (["--start-engine"], hardware),
                                  (["--demo"], ["--simulate --endpoint " + endpoint]),
                                  (["--attach-only"], [])):
            trace.write_text("")
            subprocess.run([control, "--endpoint", endpoint, "--exit-after-ms", "1300", *options],
                           env=child_env, cwd=root, check=True, timeout=6)
            calls = trace.read_text().splitlines()
            assert calls == expected, (options, calls, expected)


if __name__ == "__main__":
    main(sys.argv[1])
