# Current state — Apple Silicon Desktop preview

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
