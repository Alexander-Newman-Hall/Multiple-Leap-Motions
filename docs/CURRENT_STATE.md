# Current state — Apple Silicon Desktop preview

## 2.0.6 routed preview, 2026-11-19

Overview starts on **Routed hands**, showing each camera's applied hand selection. **Raw camera** is available for checking detections. The native arm64 Release build passed 97/97 CTest entries with `ctest --test-dir <build> --output-on-failure`. An isolated native dashboard launch with two simulated camera feeds showed one right hand in the right-only pane and both hands in the other pane. This release changes the dashboard presentation.

## 2.0.5 two-hand preview, 2026-11-19

The dashboard previously discarded every hand after the first in a camera frame. Both the raw and processed previews now draw all reported hands, with independent labels and a full-width pane for a single connected camera. The native arm64 Release build passed 97/97 CTest entries. The Qt-free SDK-off build passed 69/69. Tests replayed left-first and right-first arrivals and checked actual rendered pixels as each hand entered or left. The runtime also passed same-side, same-native-ID isolation across two simulated cameras.

The installed Mac build restored the existing profile and resumed a fresh stream from one physical camera. Its live dashboard displayed both physical hands at once, confirmed visually by the operator and in per-hand telemetry. A separate, disposable copy of the saved T2 project ran at 60 timeline frames per second through a simulated one-hand, two-hand, one-hand sequence. A physical second camera and a show-length soak remain field checks. [Mac validation](reports/MACOS_ARM64.md)

## 2.0.4 full hand skeleton, 2026-11-19

The controller and both engines support a raw, opt-in skeleton message. The SDK-enabled Apple Silicon Release build passed 95 CTest entries. The Qt-free SDK-off Release build also passed its suite. The UDP suite uses decoded packets from two simulated devices and checks 20 segment slots, raw millimeters alongside processed legacy meters, and loss/reacquisition. The documented receiver callback also decoded real UDP bytes in TouchDesigner's bundled Python using stand-ins for DAT storage and module lookup. The in-project receiver remains unverified. A separate eight-second run using the installed LeapC provider reported zero devices, so physical full-hand tracking is pending. [Protocol and decoder](FULL_HAND_OSC.md) · [Mac build report](reports/MACOS_ARM64.md)

## 2.0.3 Mac menu bar build, 2026-11-18

On Apple Silicon, the app starts its hardware engine with the dashboard hidden.
The menu bar hand opens the controls. Closing the window keeps the menu active.
Quitting the controller leaves the engine running. The native menu test exercises
opening the dashboard and restoring it after a close or minimize.
All 91 CTest entries passed. Two physical cameras streamed 3,586 frames in
the extracted-app launch and reopen check.
See the [Mac verification report](reports/MACOS_ARM64.md).

## 2.0.2 local Mac build, 2026-11-18

The Mac app starts its own hardware engine on a cold launch and reconnects to
the existing engine on subsequent launches. Login Items and schedulers can use
the same app entry point. A running engine keeps its session counters when the
UI closes. Both Send errors displays abbreviate large totals and show the full
decimal value on hover or keyboard focus.

The arm64 Release build with Qt 6.8.3 and the Hyperion 6.2.0 SDK passed all
90 CTest entries with `ctest --test-dir <build> --output-on-failure`.
Coverage includes hardware startup selection, explicit simulator and attach-only
modes, engine lifetime across GUI restarts, and the full 64-bit counter display
at minimum window width. The extracted app started real tracking without a
button press and received 5,073 frames from two physical cameras. [Mac build and verification](reports/MACOS_ARM64.md)

## Apple Silicon installation update, 2026-11-18

The public `v2.0.1` release includes the Mac app and corresponding sources
for Qt 6.8.3. The tagged Hand Bridge source is MIT-licensed.

Hand Bridge 2.0.1 builds and passes all 89 native tests on Apple Silicon with
Qt 6.8.3 and the Hyperion 6.2.0 SDK. The 2.0.0 transfer copy had 155 extra
AppleDouble files that invalidated the Qt framework signatures after extraction.
The new transfer ZIP passed strict signature verification after extraction.

Camera startup now allows three seconds for a first frame. A streaming camera
still enters recovery after 500 ms without fresh data. The injected SDK test
holds one subscription in failure and verifies that its retries back off while
the second camera keeps streaming. The [Mac report](reports/MACOS_ARM64.md)
records the hardware and release checks.

## Operator checks

The extracted app received two independent physical camera streams and a
separate OSC receiver decoded palm coordinates from both. The next field tests
are physical unplug/replug, service restart and an installation-length soak.
See the [hardware results](HARDWARE_TEST_RESULTS.md) and [installation guide](../packaging/MACOS_START_HERE.txt).

[Earlier platform validation](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/blob/3c02372976bde8b58467441f5dd25d4a43063a1a/docs/CURRENT_STATE.md)
