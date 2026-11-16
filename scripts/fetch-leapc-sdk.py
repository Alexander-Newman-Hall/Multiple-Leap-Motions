#!/usr/bin/env python3
"""Extract the pinned Hyperion 6.2 SDK for development without running its installer.

The official Windows installer and innoextract tool are verified by SHA-256 and
size before extraction. Default SDK output and download cache are git-ignored.
No SDK or tracking service is installed. See docs/reports/LEAPC_SDK_AUDIT.md.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.parse import urlsplit
from urllib.request import urlopen
import zipfile

REPOSITORY = Path(__file__).resolve().parents[1]
MANIFEST = REPOSITORY / "cmake" / "leapc-sdk-baseline.json"


def safe_path(root: Path, relative: str) -> Path:
    path = PurePosixPath(relative)
    if (not path.parts or path.is_absolute() or ".." in path.parts or
            "\\" in relative or any(":" in part for part in path.parts)):
        raise ValueError("manifest contains an unsafe relative path")
    return root.joinpath(*path.parts)


def matches(path: Path, expected: dict) -> bool:
    if not path.is_file() or path.stat().st_size != expected["bytes"]:
        return False
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest() == expected["sha256"]


def fetch(entry: dict, destination: Path) -> str:
    target = safe_path(destination, entry["path"])
    source = urlsplit(entry["url"])
    if source.scheme != "https" or source.hostname not in {"s3.eu-west-1.amazonaws.com", "constexpr.org"}:
        raise ValueError("SDK/tool source must be the pinned official HTTPS source")
    if matches(target, entry):
        return f"Verified cached {entry['path']}"
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=f".{target.name}.", suffix=".tmp",
                                         dir=target.parent, delete=False) as output:
            temporary = Path(output.name)
            received = 0
            digest = hashlib.sha256()
            with urlopen(entry["url"], timeout=60) as response:
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    received += len(block)
                    if received > entry["bytes"]:
                        raise ValueError(f"unexpected download size for {entry['path']}")
                    output.write(block)
                    digest.update(block)
            output.flush()
            os.fsync(output.fileno())
        if received != entry["bytes"] or digest.hexdigest() != entry["sha256"]:
            raise ValueError(f"SHA-256/size verification failed for {entry['path']}")
        os.replace(temporary, target)
        temporary = None
        return f"Downloaded and verified {entry['path']}"
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def extractor(entry: dict, cache: Path, windows: bool) -> Path:
    archive = safe_path(cache, entry["path"])
    print(fetch(entry, cache), flush=True)
    directory = cache / ("innoextract-windows" if windows else "innoextract-linux")
    directory.mkdir(parents=True, exist_ok=True)
    executable = directory / ("innoextract.exe" if windows else "innoextract")
    # Read only the named executable; archive paths are never extracted wholesale.
    if windows:
        with zipfile.ZipFile(archive) as package:
            payload = package.read(entry["member"])
    else:
        with tarfile.open(archive, "r:xz") as package:
            member = package.extractfile(entry["member"])
            if member is None:
                raise ValueError("verified innoextract archive lacks its executable")
            with member:
                payload = member.read()
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=directory, delete=False) as output:
            temporary = Path(output.name)
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
        temporary.chmod(0o700)
        os.replace(temporary, executable)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return executable



def extraction_command(tool: Path, staging: Path, installer: Path,
                       entries: list[dict], windows: bool) -> list[str]:
    command = [str(tool), "--extract", "--silent", "--output-dir", str(staging)]
    for entry in entries:
        safe_path(staging, entry["installer_path"])
        # innoextract 1.9 matches filters against native output separators. It
        # does not normalize CLI --include values: forward slashes select no
        # files on Windows while the extractor still returns success.
        relative = PurePosixPath(entry["installer_path"])
        include = str(PureWindowsPath(*relative.parts)) if windows else relative.as_posix()
        command += ["--include", include]
    command.append(str(installer))
    return command


def verification_failure(staging: Path, entry: dict) -> str:
    path = safe_path(staging, entry["installer_path"])
    actual = "missing"
    if path.is_file():
        actual = f"bytes={path.stat().st_size}, sha256={hashlib.sha256(path.read_bytes()).hexdigest()}"
    files = sorted(path for path in staging.rglob("*") if path.is_file())
    inventory = [{"path": path.relative_to(staging).as_posix(), "bytes": path.stat().st_size}
                 for path in files[:16]]
    return (f"extracted Hyperion SDK file failed verification: {entry['path']}; "
            f"expected bytes={entry['bytes']}, sha256={entry['sha256']}; actual {actual}; "
            f"extracted_file_count={len(files)}; file_metadata={json.dumps(inventory)}")


def acquire(manifest: dict, destination: Path, cache: Path, header_only: bool) -> None:
    selected = [entry for entry in manifest["files"] if not header_only or entry["kind"] != "binary"]
    if all(matches(safe_path(destination, entry["path"]), entry) for entry in selected):
        print("Verified cached Hyperion 6.2 SDK files", flush=True)
        return
    windows = sys.platform == "win32"
    if not windows and (sys.platform != "linux" or platform.machine().lower() not in {"x86_64", "amd64"}):
        raise ValueError("SDK extraction is supported on Windows x64 and Linux x64")
    cache.mkdir(parents=True, exist_ok=True)
    print("Preparing official Hyperion 6.2 SDK (first download is about 630 MB)", flush=True)
    print(fetch(manifest["installer"], cache), flush=True)
    tool = extractor(manifest["extractors"]["windows" if windows else "linux"], cache, windows)
    installer = safe_path(cache, manifest["installer"]["path"])
    with tempfile.TemporaryDirectory(prefix="hyperion-6.2-extract-", dir=cache) as directory:
        staging = Path(directory)
        command = extraction_command(tool, staging, installer, selected, windows)
        subprocess.run(command, check=True, timeout=300)
        # Verify every selected file before touching the existing SDK output.
        for entry in selected:
            if not matches(safe_path(staging, entry["installer_path"]), entry):
                raise ValueError(verification_failure(staging, entry))
        for entry in selected:
            target = safe_path(destination, entry["path"])
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
                    temporary = Path(output.name)
                    with safe_path(staging, entry["installer_path"]).open("rb") as source:
                        shutil.copyfileobj(source, output)
                    output.flush()
                    os.fsync(output.fileno())
                os.replace(temporary, target)
                temporary = None
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
            print(f"Extracted and verified {entry['path']}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", type=Path, default=REPOSITORY / "work" / "leapc-sdk",
                        help="development SDK directory (default: ignored work/leapc-sdk)")
    parser.add_argument("--cache-dir", type=Path, default=REPOSITORY / "work" / "sdk-cache",
                        help="ignored installer/tool download cache")
    parser.add_argument("--header-only", action="store_true",
                        help="extract the real header, notices and multi-device sample, omitting client binaries")
    args = parser.parse_args()
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        destination = args.dest.resolve()
        acquire(manifest, destination, args.cache_dir.resolve(), args.header_only)
        print(f"Ultraleap Hyperion {manifest['tracking_software_version']}; LeapC {manifest['dll_product_version']}")
        print(f"LEAP_SDK_ROOT={destination}")
        print("Development files stay out of Git and SDK-only public artifacts; tracking service is not installed.")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, tarfile.TarError, zipfile.BadZipFile) as error:
        print(f"LeapC SDK retrieval failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
