# Hand Bridge development handoff

## 2.0.7 live hand selection

The release builds on published v2.0.6 at `728da24`. `Controller.cpp` submits the selected camera’s hand filter against the current engine revision. `Main.qml` shows the acknowledged hand assignment. `EngineRuntime.cpp` reroutes that camera’s latest fresh frame and preserves the other camera’s send cadence. Regression coverage lives in `test_Controller.cpp`, `test_ControlUi.cpp` and `test_EngineRuntime.cpp`.

The Apple Silicon Release build passed all 98 CTest entries. With two simulated cameras, switching one camera changed the rendered panes and loopback scalar and skeleton OSC together. The receiver saw one skeleton loss per removed hand. The next hardware check is to repeat the changes with two physical cameras and verify the per-camera channels in TouchDesigner.

## 2.0.6 routed hand selection

Local branch `feature/full-hand-skeleton-osc` starts at `6d80da2`. `src/control/qml/Main.qml` sets the initial preview and passes the acknowledged OSC and device settings to `PreviewCanvas.qml`. The canvas draws the existing engine-provided filtered hands or, on request, raw detections. `Controller.cpp` replaces cached preview data with the acknowledged state when routing revisions change.

The native Release suite passed 97/97, including the new render and revision checks in `test_ControlUi.cpp` and `test_Controller.cpp`. The native launch check used a private endpoint with two simulated cameras, one right-only and the other set to both. The next physical two-camera check is to repeat those assignments against a receiver with both hands in view.

## 2.0.5 two-hand preview

Local branch `feature/full-hand-skeleton-osc` is based at `6d80da2`. The engine's `Preview.hpp` builds a list for each raw and processed frame. `qt_engine_main.cpp`, `Controller.cpp` and `PreviewCanvas.qml` carry each hand to its camera pane. The visible Overview can request a preview at 33 ms intervals while the 100 ms status stream continues. Older controllers can read their original first-hand fields. The worker still owns device polling and OSC. Disconnecting the controller clears its view, and destroying the control server detaches live socket callbacks before their buffers go out of scope.

The arm64 SDK-enabled and Qt-free builds passed 97 and 69 tests respectively with `ctest --test-dir <build> --output-on-failure`. The amended control, rendering and runtime tests cover the missing second hand and loss of a camera or client. The installed package passed a profile readback and a one-camera test with both hands visible on the dashboard. Next field check: repeat the hand-entry sequence under the installation's usual USB, lighting and two-camera arrangement, and compare the Ultraleap Visualizer with Hand Bridge Raw if physical jitter persists. [Mac results](reports/MACOS_ARM64.md)

## 2.0.4 skeleton output

Local branch `feature/full-hand-skeleton-osc` began at `6d80da2`. The LeapC adapter copies the 6.2 hand fields. The encoder in `src/transport/osc/SkeletonOsc.cpp` builds one fixed-layout live pose or one short loss event. `EngineRuntime` selects raw skeletons by alias, side and native ID. Controller settings expose the feature under OSC Output. The Apple Silicon package includes the Python decoder and [wire/rig guide](FULL_HAND_OSC.md).

Run `ctest --test-dir <build> --output-on-failure` on the arm64 Release build. The SDK-enabled suite passed 95/95, including the controller click, decoded loopback output, SDK field copy and same-side hand identity tests. The published example callback decoded both simulated hands and loss events under TouchDesigner's bundled Python 3.11.15, with stand-ins for the DAT's lookup and storage calls. An in-project OSC In DAT test remains open. The native library loaded on the eight-second real-provider probe. It enumerated zero devices. The next field check is a visible hand through the receiver while curling one digit and rolling the wrist. [Mac verification](reports/MACOS_ARM64.md)

## 2.0.3 Mac menu bar build

Branch `feature/macos-menu-bar` starts from public `v2.0.2` (`09b04da`).
`src/control/MacMenuBar.mm` owns the AppKit status item and window actions.
`control_main.cpp` controls initial visibility and `scripts/package-macos.py`
marks the bundle as a menu bar agent. The engine continues to own camera
tracking and OSC independently of the window. The operator guide includes
instructions for opening the dashboard and quitting the menu bar controller.
All 91 CTest entries passed. On the extracted build, two physical cameras
streamed during the hidden launch and Finder-style reopening of the dashboard.
The remaining field check is a scheduled cold login and show-length soak.
See the [Mac verification report](reports/MACOS_ARM64.md) for the results.

## 2.0.2 local Mac build

The branch `feature/macos-unattended-start` is based on the public 2.0.1
source at `a24b19c`. The startup policy lives in `control_main.cpp` and
`Controller.cpp`. The engine publishes exact OSC totals in `qt_engine_main.cpp`;
`Main.qml` displays them at the available width. The packager produces an
arm64 app for manual, scheduled or login-item launch. Scheduling is configured
by the operator on the installation Mac.

The arm64 suite passed all 90 CTest entries. `MacControllerStartup` exercises
the actual executable against a stub engine. `ControlUi` exercises the
dashboard with totals up to 18446744073709551615. The next deployment check
is a cold login with the operator's scheduler and OSC receiver, followed by
a show-length soak. [Mac verification](reports/MACOS_ARM64.md)

## 2.0.1 public release

The Apple Silicon release contains the MIT-licensed controller and independent
tracking engine. A user can run the compiled app or build the tagged source.
Qt 6.8.3 corresponding sources accompany the binary ZIP.

`EngineRuntime` allows three seconds for a camera's first frame and 500 ms for
a fresh frame on an established stream. A renewed SDK subscription starts a
new camera-specific retry schedule. The native suite passed all 89 tests.
A separate OSC receiver also decoded palm coordinates from both physical
cameras. The next field check is physical unplug/replug of one camera while
the receiver continues receiving from the other one.

[Mac build and package verification](reports/MACOS_ARM64.md) · [Hardware results](HARDWARE_TEST_RESULTS.md)

[Previous development handoff](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/blob/3c02372976bde8b58467441f5dd25d4a43063a1a/docs/SESSION_HANDOFF.md)
