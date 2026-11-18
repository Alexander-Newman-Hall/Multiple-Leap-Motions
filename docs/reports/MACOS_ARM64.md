# Native Apple Silicon build and validation

Hand Bridge 2.0.1 targets macOS 12 or newer on Apple Silicon. The controller
and engine are arm64. Qt 6.8.3 is bundled. Real cameras require the separately
installed Ultraleap Hyperion 6.2.0 runtime for Apple Silicon.

## Verification

The release build passed all 89 native tests, including delayed first frames,
missing frames, repeated subscription failures and reattachment. Two physical
Leap Motion Controller 2 cameras streamed continuously for 153 seconds. The
loopback receiver decoded palm positions from both. The engine recorded zero
reconnect attempts, SDK poll errors and OSC send errors.

Earlier cold-attachment trials recorded 681–878 ms from subscription to the
first frame. One early dual-camera startup left a camera silent. Connecting
the cameras in sequence and reconnecting the silent camera restored both
streams. A standalone probe also observed one physical unplug/reconnect with
continued frames from the other camera. Physical recovery on the 2.0.1 release
and a show-length soak remain open. [Release hardware results](../HARDWARE_TEST_RESULTS.md)

The public ZIP passed archive-integrity and strict code-signature checks after
extraction. Its controller and engine contain the same arm64 machine code as
the binaries used for the camera test. The public bundle also passed the
installed-client preflight and the simulated UDP stall/recovery test. The
application uses a local ad hoc signature. [Installation instructions](../../packaging/MACOS_START_HERE.txt)

The SHA-256 digest of `Hand-Bridge-2.0.1-Apple-Silicon.zip` is
`730ebc629bc2211131ccd47e239ffd8a5b504c351a2297763eede1f649bbd2db`.

## Rebuild the local preview

From the repository root, install the Xcode Command Line Tools, CMake, Ninja,
Python 3.9 or newer, Qt 6.8.3 for macOS and the Ultraleap SDK. Set `qt` to
your Qt macOS prefix, for example `qt=/path/to/Qt/6.8.3/macos`.

```sh
sdk='/Applications/Ultraleap Hand Tracking.app/Contents/LeapSDK'
cmake --preset macos-arm64-control -B out/mac-desktop -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$qt" -DHANDBRIDGE_WITH_LEAPC=ON -DLEAP_SDK_ROOT="$sdk"
cmake --build out/mac-desktop --parallel 5
ctest --test-dir out/mac-desktop --output-on-failure
python3 scripts/package-macos.py out/mac-desktop "$qt" "out/Hand Bridge.app"
python3 scripts/archive-macos.py "out/Hand Bridge.app" "out/Hand-Bridge-2.0.1-Apple-Silicon.zip"
"out/Hand Bridge.app/Contents/MacOS/handbridge-engine" --check-leapc
open "out/Hand Bridge.app" --args --start-engine
```

The ZIP contains the application and its operator guide. The archive helper
preserves Qt's framework symlinks, excludes Finder metadata files, and runs
strict code-signature verification on a fresh extraction before publishing.

[Historical macOS feasibility checks](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/blob/3c02372976bde8b58467441f5dd25d4a43063a1a/docs/reports/MACOS_ARM64.md)
