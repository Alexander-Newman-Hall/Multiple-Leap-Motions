# Windows + Ultraleap + TouchDesigner physical acceptance test plan

**Status:** Test plan only. No physical tests have been performed in preparing this kit.

## Before testing

Record: Git commit SHA, build artifact SHA/version, Windows edition/build, MSVC runtime/Qt version if relevant, installed Hyperion/LeapC service versions, controller model/firmware, USB controller/hub and power arrangements, OSC IP/port and TouchDesigner build. Use serial pseudonyms in shared records.

Setup: One Windows PC, two Leap Motion Controller 2 devices, an appropriate USB connection for both, TouchDesigner and Hand Bridge headless engine/UI. Configure OSC receiver at `127.0.0.1` and actual configured port (legacy C++ fallback currently 9000, subject to saved settings).

### Gate A: Single-camera basics

- [ ] Verify Ultraleap service is actually running. Launch engine headlessly.
- [ ] One camera appears with stable serial/alias and subscribed state.
- [ ] Move left hand and right hand separately; confirm source joint values and handedness update.
- [ ] Remove hands; frame arrival remains healthy even with zero visible hands; no stale data held indefinitely.
- [ ] Restart app and confirm chosen filters, aliases and OSC port persist.
- [ ] Inspect OSC In DAT/CHOP for actual address/value/types, not merely a send counter.

### Gate B: Two simultaneous controllers

- [ ] Attach both cameras; assert separate device serial/alias mapping and independent frame counters.
- [ ] A hand in one camera does not spuriously overwrite the other's alias/channel.
- [ ] Confirm simultaneous output from both devices, with correct device and handedness labels.
- [ ] Disable Camera A output; Camera B continues sending unmodified.
- [ ] Reassign one camera left/right filter; confirm matching output channels change and assignment persists.
- [ ] Close/reopen native UI with engine still running; TouchDesigner receives unchanged stream.
- [ ] Kill GUI process; engine survives and is reattachable after relaunch.

### Gate C: Failure reproduction and recovery

- [ ] Physically unplug A while B runs; B remains usable, A marked disconnected, A's stale/validity state cleared.
- [ ] Replug A; reopen/resubscribe with original stable alias, output resumes without restarting engine.
- [ ] Repeat with B and reversed attachment order.
- [ ] Restart Ultraleap Tracking Service in a controlled test environment; engine detects loss and recovers correctly or reports a bounded actionable failure. Do not stop an organization's shared service without permission.
- [ ] Exercise USB sleep/power management scenario if applicable.
- [ ] Stress queue with high CPU/background load; dropped-frame counters reflect reality and stay finite; prevent unnoticed permanent stall.
- [ ] Simulate invalid port and correct it; UI and logs describe sender state truthfully.
- [ ] Restart TouchDesigner only; verify OSC sender continues and TD reattaches after the listener restarts.
- [ ] Confirm no hands is *not* classified as a disconnected device.
- [ ] Use diagnostic export to capture service, device, queue and OSC stages if a silent stall happens.

### Gate D: Processing and OSC controls

- [ ] Verify default legacy OSC addresses, lowercase `left`/`right` where current source uses them, float values, active port and units vs golden tests.
- [ ] Switch raw/processed output for one channel; confirm numeric change and no cross-device leakage.
- [ ] Verify smoothing is time-based and does not create giant lag or jump after reacquisition.
- [ ] Verify offsets/transforms and units on palm, wrist, finger values and correct axis conventions.
- [ ] Configure pinch threshold+hysteresis and validate stable transitions without oscillation.
- [ ] Save preset; restart engine, then reopen UI; compare exact output and readback.
- [ ] Optional bundled messages are received and parsed by OSC In DAT/CHOP; large bundles split into bounded packets.

### Gate E: Endurance and operations

- [ ] At least one multi-hour two-camera soak, with logger size, memory usage, CPU, mean/max frame ages and dropped frames captured. Choose duration appropriate to deployment and document exact value.
- [ ] Repeat at least 10 unplug/replug and service-loss recovery cycles; report counts and any failures.
- [ ] Run with UI closed most of the time; open briefly for diagnostics; confirm no tracking disturbance.
- [ ] Validate ability to shut down cleanly and reopen without orphaned device handles or duplicate output.
- [ ] Document cold boot/autostart behavior if required by an installation.

## Results log — copy one row per test

| Date/time (local) | Commit | Test ID | Environment | Result | Evidence / metrics | Defect link |
|---|---|---|---|---|---|---|
| — | — | — | UNTESTED | NOT RUN | — | — |

Use statuses `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`. File test reports in `docs/HARDWARE_TEST_RESULTS.md` or dated files under `docs/reports/` and update `docs/CURRENT_STATE.md`.

## Safety/realism notes

- Never send high-frequency debug logs on the LeapC tracking callback thread during measurement.
- On UDP, a sender reporting success does not establish receiver receipt; use OSC In DAT/CHOP or a separate packet capture.
- A GUI preview value is not proof OSC was transmitted; verify the real receiver.
- Without connected physical devices, all items involving USB/Hyperion must remain NOT RUN or BLOCKED, not PASS.
