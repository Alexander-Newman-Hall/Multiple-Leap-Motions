"""Assemble a local Apple Silicon Hand Bridge app from a Release control build."""
import argparse
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile


def package(build, qt, destination):
    repo = Path(__file__).resolve().parents[1]
    if destination.exists() or destination.is_symlink():
        raise SystemExit(f"Destination already exists: {destination}")
    if destination.suffix != ".app":
        raise SystemExit("Use a destination ending in .app")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
        app = Path(temporary) / destination.name
        contents = app / "Contents"
        programs = contents / "MacOS"
        resources = contents / "Resources"
        programs.mkdir(parents=True)
        resources.mkdir()
        for name in ("handbridge-control", "handbridge-engine"):
            source = build / name
            slices = subprocess.check_output(["lipo", "-archs", source], text=True).strip()
            if slices != "arm64":
                raise SystemExit(f"Expected native arm64 for {name}, found {slices}")
            shutil.copy2(source, programs / name)
        with (contents / "Info.plist").open("wb") as stream:
            plistlib.dump({
                "CFBundleInfoDictionaryVersion": "6.0",
                "CFBundlePackageType": "APPL",
                "CFBundleIdentifier": "art.newman.handbridge.control",
                "CFBundleName": "Hand Bridge",
                "CFBundleDisplayName": "Hand Bridge",
                "CFBundleExecutable": "handbridge-control",
                "CFBundleIconFile": "HandBridge.icns",
                "CFBundleShortVersionString": "2.0.6",
                "CFBundleVersion": "2.0.6",
                "LSUIElement": True,
                "LSMinimumSystemVersion": "12.0",
                "NSHighResolutionCapable": True,
            }, stream)
        subprocess.run(["sips", "-s", "format", "icns", repo / "assets/dexteraICO.ico",
                        "--out", resources / "HandBridge.icns"], check=True)
        shutil.copytree(repo / "packaging/licenses", resources / "licenses")
        shutil.copy2(repo / "LICENSE", resources)
        shutil.copy2(repo / "packaging/macos/qt-sources.json", resources)
        shutil.copy2(repo / "packaging/macos/Third Party Notices.txt", resources)
        subprocess.run([qt / "bin/macdeployqt", app, f"-executable={programs / 'handbridge-engine'}",
                        f"-qmldir={repo / 'src/control/qml'}", "-appstore-compliant",
                        "-codesign=-"], check=True)
        with (contents / "Frameworks/QtCore.framework/Resources/Info.plist").open("rb") as stream:
            qt_version = plistlib.load(stream)["CFBundleVersion"]
        source_version = json.loads((resources / "qt-sources.json").read_text())["version"]
        if qt_version != source_version:
            raise SystemExit(f"Bundled Qt {qt_version} does not match its source manifest ({source_version})")
        # Qt copies available SQL drivers even for apps that never open a DB.
        # The optional Mimer driver requires a separately installed Mimer SDK.
        mimer = contents / "PlugIns/sqldrivers/libqsqlmimer.dylib"
        if mimer.exists():
            mimer.unlink()
            subprocess.run(["codesign", "--force", "--sign", "-", app], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", app], check=True)
        app.rename(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("qt_prefix", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    package(args.build.resolve(), args.qt_prefix.resolve(), args.destination.absolute())
