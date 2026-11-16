# Hyperion 6 SDK / Windows Release evidence

Final validated source: **c8596f20b35bb0e4d09446933c543f689417f36c**. Date: 2026-11-16 UTC.
Branch: codex/handbridge-leapc-release. Final documentation-only follow-up changes
no compiled application/test/package code.

## Genuine provider and build

The official current LMC 2 download page advertised Hyperion 6.2.0 when checked.
Its629594088-byte Windows installer was SHA-256 verified and only SDK files were
extracted; the installer was not run. Header/SDK/API provenance and licence
boundary: [LEAPC_SDK_AUDIT](LEAPC_SDK_AUDIT.md). The DLL file version is 6.2.0.0,
actual ClientLibrary API version 6.2.0. Both client and connected ServerLibrary
must be 6.2.0 or a newer 6.x before healthy service/device enumeration. Required
API signatures/calling conventions come from the genuine 6.2 header.

Linux GCC 14.2/Qt 6.8.3 native compile/full suite:82/82 PASS, no skips,20.48 s.
The 21 production-adapter cases inject SDK entry points, never a fake header.
They cover independent subscriptions, owned copies/native IDs, no hands, device/
service loss, stale/manual restart, duplicate serial/reference reuse, failed API
calls/handshake, old/unknown service rejection and supported 6.x version reporting.
Project ASan/UBSan adapter/process subset:34/34 PASS, leak checking,0.59 s; no
instrumentation/claim for third-party Qt or the actual Windows SDK binary.
SDK fetch helper:7/7 offline cases plus real Linux/Windows extraction verification.

## Successful Windows gates

- [Release SDK build, deployment and smoke](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905), job 133010802213.
- [SDK-free core Windows/Ubuntu](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709893).
- [Native controller SDK-off Windows/Ubuntu](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709916).

Windows Server 2022/MSVC 19.44.35230:82 registered,**81 PASS +1 expected POSIX-only
broadcast-permission skip**, zero failures,32.03 s. Three additional ControlIntegration
until-fail runs all passed 19.09/19.08/19.07 s (57.25 s total). No until-pass retries or
relaxed assertions conceal failures. Real DLL probe returned:

```json
{"provider":"leapc","version":"6.2.0.0","client_api_version":"6.2.0","hardware_verified":false}
```

The actual path pointed to the verified external development SDK. A 3600 ms genuine
client run without tracking service/cameras made 2 connection attempts, retained
an actionable handshake timeout, reported service_connected=false/devices={} /
packets_sent=0, and exited cleanly before the 15 s outer deadline. This exercises
real Create/Open/Poll/recovery/cleanup, not real service/camera tracking.

The extracted ZIP passed**16 checks**: missing/relative/non-SDK/x 86 DLL rejection;
genuine external 6.2 DLL/API/exports; Unicode/space path round-trip; offline SDK
lifecycle; real headless OSC; app-local Qt/Release CRT paths; no rendering modules
in the engine; actual qwindows GUI render; OSC after normal close/kill/reopen;
and clean developer search paths. PATH contained only Windows system locations,
Qt/QML/CMake/SDK search variables were cleared, and CWD was outside the package.
Actual screenshots are [Overview](../screenshots/windows-overview.png) and
[Processing](../screenshots/windows-processing.png); runner available-window size
was 1100×749, with normal scrolling for lower controls. These are simulated-hand
screenshots and explicitly carry that label.

## Published artifact / independent inspection

[Final portable artifact](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43778709905/artifacts/14187119668), ID14187119668,108189660 bytes outer ZIP.
It contains the application ZIP, SHA-256 file and package-manifest.json.
The manifest identifiesc8596f20b35bb0e4d09446933c543f689417f36c, Release, Qt 6.8.3,
source_dirty=false, leapc_adapter_compiled=true, leapc_runtime_included=false,
hardware_verified=false and header hash
27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765.
The inner ZIP/checksum/manifest and absence of LeapC.h/.lib/.dll were independently
inspected. Inner ZIP SHA-256:
`24a3511a28722a78f4a8e90088e67b7ef107108bc45a8c39af5345a2a512438e`
(108752626 bytes). Qt DLLs are from qtbase/qtdeclarative, whose exact corresponding-source
archives/licenses are included; Microsoft CRT is from the VS x64 redist directory.
No SDK binary or Debug CRT is distributed. Use WINDOWS_RELEASE/LEAPC_SETUP guides.

## Corrected findings / remaining boundary

First Windows run 43777007955 failed before configure: innoextract matches native
Windows include separators, while the helper supplied POSIX paths. The corrected
helper keeps every SDK digest unchanged, normalizes filters and adds regressions.
A later independent artifact review found generated root JUnit outputs caused a
truthful-but-misleading dirty-checkout flag; explicit generated-file ignores and
an additional clean-source CI gate corrected it. Final run above passed both gates.

This is an unsigned portable developer preview, not a camera-accepted release.
No actual Hyperion service, physical controller, USB hotplug, TouchDesigner receipt,
consumer Windows 10/11 fresh install or hardware soak is claimed. Next task is the
local two-camera acceptance in HARDWARE_TEST_PLAN/HARDWARE_TEST_RESULTS.
