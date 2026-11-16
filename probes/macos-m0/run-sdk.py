"""Inspect the extracted official Mac SDK and run the owned diagnostic only."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

root = Path('work/macos-m0/sdk')
out = Path('work/macos-m0')

def run(args, expected=0, timeout=30):
    print('+', ' '.join(map(str, args)), flush=True)
    result = subprocess.run(list(map(str, args)), timeout=timeout)
    print('exit=', result.returncode, 'expected=', expected, flush=True)
    if result.returncode != expected:
        raise RuntimeError('unexpected exit status')

headers = list(root.rglob('LeapC.h'))
libraries = sorted({p.resolve() for p in root.rglob('*LeapC*.dylib') if p.is_file()})
print('headers:', headers, 'resolved dylibs:', libraries, flush=True)
if len(headers) != 1 or len(libraries) != 1:
    sys.exit('BLOCKED: SDK layout ambiguous; inspect before selecting files')
header, library = headers[0], libraries[0]
for path in [header, library]:
    print('SHA256', hashlib.sha256(path.read_bytes()).hexdigest(), path, flush=True)
# No vendor header, library, sample source, or package is uploaded as an artifact.
# Inspection tools report only binary metadata/symbol names.
for command in [['file', library], ['lipo', '-info', library], ['otool', '-L', library], ['nm', '-gU', library]]:
    run(command)
for p in sorted(root.rglob('*')):
    if p.is_file() and any(word in p.name.lower() for word in ['readme', 'license', 'licence', 'cmakelists', 'eula']):
        print('sample/terms inventory:', p, 'sha256=', hashlib.sha256(p.read_bytes()).hexdigest(), flush=True)
run(['xcrun', 'clang++', '-std=c++20', '-Wall', '-Wextra', '-pthread', '-I', header.parent,
     'probes/macos-m0/sdk-diagnostic.cpp', '-o', out/'sdk-diagnostic'])
exe = (out/'sdk-diagnostic').resolve()
run([exe, out.resolve()/'definitely-missing.dylib', '--load-only'], expected=2)
run([exe, library, '--load-only'])
# The package is expanded only, never installed. Do not stop/start any service.
# Fail if an unexpected real connection exists rather than relabel it no-service.
run([exe, library], expected=3, timeout=15)
print('M0 OFFLINE CHECKS PASS; PHYSICAL TWO-CAMERA TESTS NOT RUN', flush=True)
