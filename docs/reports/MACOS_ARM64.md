# Native Apple Silicon build and validation

Hand Bridge targets macOS 12 or newer on Apple Silicon. The controller
and engine are arm64. Qt 6.8.3 is bundled. Real cameras require the separately
installed Ultraleap Hyperion 6.2.0 runtime for Apple Silicon.

## Version 2.0.7 live hand selection

The native SDK-enabled Release build passed 98/98 CTest entries. A separate Qt dashboard test used two simulated cameras, each observing both hands. With the second camera set to Both, changing the first through Right, Left, Disabled and Both preserved the second camera’s preview and OSC output. For each selection, an independent UDP decoder saw the same active hands in the legacy palm channel and the full-hand skeleton messages as the routed canvas displayed.

## Version 2.0.5 two-hand preview

The native SDK-enabled Release build passed 97/97 tests on Apple Silicon. The Qt-free SDK-off Release build passed 69/69. Rendered-pixel tests cover each hand entering first, both present together, and each leaving independently in raw and processed views. Synthetic frames with the same side and same native ID on two cameras kept independent OSC addresses through a device disconnect.

In a disposable copy of the saved T2 project, two simulated streams on one camera alias drove both calibrated sides. The TouchDesigner timeline measured 60.1 Hz with only the left hand, 60.0 Hz with both, and 60.0 Hz after the right hand left. The test used the existing OSC In DAT and CHOP on the same isolated loopback port. The video input was disabled in the test copy. These numbers describe timeline callbacks and do not measure GPU presentation latency.

During a separate 60-second physical check of version 2.0.4, one camera processed 6,595 frames without reconnecting. The preview registered a moving hand, including 323 mm of palm X travel. The vendor service logged repeated camera gain warnings, which continued after an acknowledged subscription restart. The earlier bridge log also recorded refused OSC sends when the receiver was absent. Neither observation identifies the cause of every visual stall. Installing 2.0.5 restored the saved settings and a fresh physical stream. The operator ZIP passed strict signature verification after extraction. SHA-256 is `7d9d2ff7740e8f9d5ddcf9e73ede9d434c4c8c35519e8acd6c687339108b35dd`.

After the upgrade, `dev1` processed 6,950 frames over a one-minute check with zero additional reconnects. All 603 previews in that first window were empty. In the subsequent operator check, one physical camera reported left-only, right-only and simultaneous left/right poses. At least 85 telemetry snapshots contained both hands. The installed dashboard displayed two separately positioned palms labeled LEFT and RIGHT and the operator confirmed seeing both. A second physical device was unavailable for this run.

## Version 2.0.4 full hand skeleton build

The arm64 Release build passed all 95 CTest entries on macOS 26.6.2 with
Qt 6.8.3 and the Hyperion 6.2.0 SDK. Tests cover the controller switch,
SDK conversion, skeleton fields, per-hand identities, loss, routing and
reception of real UDP datagrams. The extracted app passed strict recursive
code-signature verification and loaded the installed 6.2.0 LeapC client.

An independent loopback receiver captured the packaged simulator using the
stall scenario. It received 66 left-hand poses and 118 right-hand poses,
plus three loss events. It verified every joint endpoint and rotation in
both hands, 278 arguments in each live message and a 1,444-byte payload
for the default aliases. Legacy scalar messages arrived on the same port.
The Python decoder shipped in the ZIP read both hands and the loss event.
The real provider reported zero connected cameras during this release check.
Physical skeleton capture and retargeting in the receiving application
remain field checks. [Skeleton protocol and mapping](../FULL_HAND_OSC.md)

The SHA-256 of the verified Apple Silicon ZIP is
`4a45fc6b24dfb7918988763d367e95118f53b2c0bb69aeff651019f18f836995`.

## Version 2.0.3 menu bar build

The Apple Silicon Release build passed all 91 CTest entries on macOS 26.6.2
with Qt 6.8.3. `MacMenuBar` invoked the Open Hand Bridge action through the
native AppKit menu, including after a window close and a minimize. The control
integration test checked UDP output across controller lifecycles.

The ZIP passed integrity checks and strict signature verification after
extraction. Its bundle has `LSUIElement` enabled, and the arm64 controller
targets macOS 12 or newer. App ZIP SHA-256 is
`8ac4cf9beaf8558f79518653df55045f4eeedbc02129ec9405e27bd7bbfbe68b`.

The extracted app launched with zero visible dashboard windows and left the
current application in front. Hyperion 6.2.0 connected in 1.62 seconds.
Two physical cameras streamed 3,586 frames. Reopening the same bundle through
LaunchServices brought one dashboard window to the front, using the same
controller and engine processes. The engine recorded zero send errors. No
hand-derived OSC packets were observed in this check.

## Version 2.0.2 local build

The Release build passed all 90 CTest entries on Apple Silicon. The cold-launch
test observed the LeapC preflight followed by the hardware engine invocation.
Separate cases cover explicit startup, simulator selection and attach-only
monitoring. The UI test exercises counters through 18446744073709551615 with
the window at its minimum size.

The ZIP passed its integrity check and strict code-signature verification on the
extracted app. Its SHA-256 is
`219c7a6fdf80a686aa2eda3e65e6ff2992282620b58a7882664d3159809a970a`.

Launching the controller from that extraction with an isolated control endpoint
connected to the real Hyperion 6.2.0 service in 2.33 seconds. Two physical
cameras produced 5,073 tracking frames with no operator input. Closing and
reopening the controller reconnected to the same engine process. No hand-derived
OSC packets were observed during this launch check. A scheduled login and a
show-length soak remain deployment checks.

## Version 2.0.1 verification

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
python3 scripts/archive-macos.py "out/Hand Bridge.app" "out/Hand-Bridge-2.0.4-Apple-Silicon.zip"
"out/Hand Bridge.app/Contents/MacOS/handbridge-engine" --check-leapc
open "out/Hand Bridge.app"
```

The ZIP contains the application and its operator guide. The archive helper
preserves Qt's framework symlinks, excludes Finder metadata files, and runs
strict code-signature verification on a fresh extraction before publishing.

[Historical macOS feasibility checks](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/blob/3c02372976bde8b58467441f5dd25d4a43063a1a/docs/reports/MACOS_ARM64.md)
