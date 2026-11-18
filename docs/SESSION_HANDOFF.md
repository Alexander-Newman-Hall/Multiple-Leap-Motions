# Hand Bridge development handoff

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
