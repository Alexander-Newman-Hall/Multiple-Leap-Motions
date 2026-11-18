"""Make and verify an operator ZIP from a signed Hand Bridge app bundle on macOS."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def verify_app(app):
    for name in ("handbridge-control", "handbridge-engine"):
        program = app / "Contents/MacOS" / name
        if not program.is_file() or not os.access(program, os.X_OK):
            raise SystemExit(f"Missing or non-executable program: {program}")
    subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", app], check=True)


def archive(app, destination):
    if app.suffix != ".app" or not app.is_dir():
        raise SystemExit(f"Supply a macOS .app bundle: {app}")
    if destination.suffix != ".zip" or app in destination.parents:
        raise SystemExit("Choose a .zip destination outside the app bundle")
    if destination.exists() or destination.is_symlink():
        raise SystemExit(f"Destination already exists: {destination}")

    guide = Path(__file__).resolve().parents[1] / "packaging/MACOS_START_HERE.txt"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory(dir=destination.parent) as publishing:
        work = Path(directory)
        stage = work / "stage"
        staged_app = stage / "Hand Bridge.app"
        shutil.copytree(app, staged_app, symlinks=True,
                        ignore=shutil.ignore_patterns("._*", ".DS_Store", "__MACOSX"))
        shutil.copyfile(guide, stage / "Start Here.txt")
        verify_app(staged_app)

        # Qt frameworks depend on relative symlinks. AppleDouble sidecars next
        # to those links can invalidate an otherwise intact signed framework.
        zipped = Path(publishing) / "transfer.zip"
        subprocess.run(["/usr/bin/ditto", "-c", "-k", "--norsrc", stage, zipped], check=True)
        unpacked = work / "extracted"
        subprocess.run(["/usr/bin/ditto", "-x", "-k", zipped, unpacked], check=True)
        verify_app(unpacked / staged_app.name)
        if (unpacked / "Start Here.txt").read_bytes() != guide.read_bytes():
            raise SystemExit("Installation guide failed archive verification")

        # Same-volume rename publishes only the complete ZIP. -n keeps an
        # earlier archive if two packaging jobs finish at the same destination.
        subprocess.run(["/bin/mv", "-n", zipped, destination], check=True)
        if zipped.exists():
            raise SystemExit(f"Destination already exists: {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    archive(args.app.resolve(), args.destination.parent.resolve() / args.destination.name)
