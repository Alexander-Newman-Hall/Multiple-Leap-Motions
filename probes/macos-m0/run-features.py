"""Run both Apple library probes independently; preserve actual failures."""
from pathlib import Path
import subprocess

out = Path('work/macos-m0')
failed = False
for name in ('threading', 'snapshot'):
    commands = [
        ['xcrun', 'clang++', '-std=c++20', '-pthread', f'probes/macos-m0/{name}.cpp', '-o', str(out/name)],
        [str(out/name)],
        ['otool', '-L', str(out/name)],
    ]
    for command in commands:
        print('+', ' '.join(command), flush=True)
        try:
            result = subprocess.run(command, timeout=60)
            print('exit=', result.returncode, flush=True)
            if result.returncode:
                failed = True
                break
        except subprocess.TimeoutExpired:
            print('FAIL: timeout', flush=True)
            failed = True
            break
    else:
        print(name, 'COMPILE/LINK/RUN PASS', flush=True)
raise SystemExit(1 if failed else 0)
