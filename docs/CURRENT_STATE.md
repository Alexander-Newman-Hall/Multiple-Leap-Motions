# Current state — Hyperion 6 adapter and Windows portable preview

Branch: `codex/handbridge-leapc-release`, based on the preserved native-controller
branch at `3825ecd0d7acf35be6a8a7d164ebef01bd25d790`.
The original master/handbridge-v2 and prior feature branches are untouched.
Adapter implementation commit: `5550359`. This checkpoint accompanies Release
packaging; final source commits and
Windows workflow results will be recorded after the Release deployment gate runs.

## Implemented in this increment

- Real LeapC ITrackingSource, genuine Hyperion6.2 SDK declarations and injected
  API seam. One poll owner, multi-device-aware connection, explicit subscriptions,
  serial identity, owned observations, native hand IDs and per-device recovery.
- Bounded poll/event work, three-second handshake deadline, capped retries,
  service/device cleanup and targeted stale/manual device restart.
- Explicit latest-v6 policy: client API and connected server-library versions
  must each be6.2.0 or newer within6.x. Unknown/older/other-major versions cannot
  become healthy service state. Real versions are queried using LeapGetVersion.
- Safe absolute Windows DLL loading, required-export checks, nontracking
  --check-leapc, actual DLL/API/service diagnostics and clear missing-SDK errors.
- Separate GUI Start engine / Start simulator actions; asynchronous preflight
  and detached engine lifecycle. Actual service/DLL versions and retained errors
  appear separately. Closing the GUI still does not own tracking lifetime.
- Native hand-ID changes reset only that hand's smoothing/deadzone/hysteresis.
  Legacy OSC, raw-mm defaults, localhost:9000 and schema2 remain unchanged.
- Release preset/deployment scripts, app-local Qt/QML/plugins + Microsoft CRT,
  corresponding Qt source/licenses, package manifest and extracted-ZIP smoke.
  Windows deployment results remain pending below until actually run.

## Exact SDK baseline

Official Hyperion6.2.0 Windows installer, SHA-256
`e219438dc5031af3dc75e115f277c4e6e33475bad21fee1e90a278e77f5fadf1`.
Actual client DLL:6.2.0.0-c98d293a; header SHA-256
`27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765`.
The helper verifies/extracts selected SDK files without executing the installer.
The earlier Unreal-plugin5.30 research baseline is superseded and not a release
validation dependency. No proprietary SDK file is committed or bundled.
See LEAPC_SETUP.md and reports/LEAPC_SDK_AUDIT.md.

## Actual verification so far

| Scope | Result |
|---|---|
| Cloud GCC14.2 / Qt6.8.3 / actual Hyperion6.2 header | Configure/build PASS |
| Full SDK-enabled native suite | **82 passed, zero failures/skips,20.48s** |
| Genuine-SDK-type injected adapter tests |21 passed; no hardware/SDK binary calls |
| SDK-off native suite before additive service-version fields |61 passed, zero skips,20.33s |
| Real Hyperion6.2 installer extraction/hash checks |PASS; not installed/executed |
| PowerShell packaging parse/helper checks |PASS; actual Windows deployment pending |
| First Windows Release run43777007955 |SDK extraction failed before compile; corrected Windows-native include filters; rerun pending |
| Windows real client DLL load/version/API |Pending corrected Release workflow |
| Extracted Windows ZIP, no developer PATH |Pending Release workflow |
| Physical cameras/Hyperion recovery/USB/TouchDesigner |**UNTESTED ON HARDWARE** |

Native command: cmake --build out/build/leapc-control --parallel2, followed by
ctest --test-dir out/build/leapc-control --output-on-failure. The build uses
HANDBRIDGE_WITH_LEAPC=ON and the exact extracted6.2 header; offscreen/software Qt
is used in cloud. Full reproduction is in SESSION_HANDOFF and Windows guides.
The previous controller baseline at ebb3bd9 had green Windows/Ubuntu CI; its
results do not substitute for this increment's Windows SDK/package gate.

## Remaining limits and next task

No physical multi-device proof, USB/service recovery observations, TouchDesigner
receipt, consumer Windows10/11 clean-machine acceptance or hardware soak. Current
processing/channel settings remain global; routing/identity is per-device. No
fusion, OSCv2/bundles, full3D/all-joint preview, installer or automatic startup.
Actual user-installed SDK/service versions are unknown until queried locally.

After the cloud Release gate passes, the single next task is local Windows
Hyperion6.2 two-camera acceptance using HARDWARE_TEST_PLAN.md: record versions,
actual OSC receipt, unplug/replug/service recovery and continued output with the
GUI closed. Record PASS/FAIL/BLOCKED; never infer a camera result from these mocks.
