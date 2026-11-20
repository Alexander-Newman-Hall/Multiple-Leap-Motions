# Windows / Hyperion 6 / TouchDesigner physical acceptance plan

**Status: UNTESTED ON HARDWARE.** The real adapter is compiled and its lifecycle
is tested using injected functions with official SDK types. Software-only Windows
DLL loading/version checks, a missing-service test, portable ZIP smoke and mock
frames do not establish physical camera, USB, service-recovery or TD behavior.
Record their results separately from every physical checkbox below.

## Prepare the actual test PC

Record the exact source commit, package manifest/hash, Windows edition/build,
Qt/MSVC runtime versions, installed client DLL path/file/API version, connected
Hyperion service version, controller model/firmware/count, USB port/controller/hub
and power arrangement, profile, OSC destination and TouchDesigner version. Redact
serials and private paths before sharing diagnostics.

Use Windows 10/11 x64, two Leap Motion Controller 2 devices and the official
Hyperion **6.2.0 or newer 6.x** runtime/client. Both client and connected service
must pass the implemented version gate; unknown/older/different-major versions
must remain unavailable. Follow [SDK setup](LEAPC_SETUP.md), the Windows build/
portable guide and [current state](CURRENT_STATE.md) for the exact tested build.
No SDK binary is bundled with the portable preview.

Run the documented `--check-leapc` probe with the intended absolute DLL path first.
That verifies client loading/required exports/API version only; it does not connect
to the tracking service or observe cameras. Then launch the real engine, or choose
**Start engine** in the controller. **Start simulator** remains a separate explicit
mode; its results cannot fill the physical checklist. Confirm actual provider and
service-version diagnostics before proceeding.

Configure TouchDesigner OSC In DAT/CHOP for `127.0.0.1` and the exact active port
(default 9000; a saved valid different port is preserved). Inspect OSC In DAT
addresses/types before interpreting CHOP channels. A sender counter is not receipt.

## Gate A — one real camera

- [ ] In Overview, introduce one hand, add the other beside it, then withdraw
      the first while moving the remaining hand. Repeat in reverse order.
      Check both palm labels on the same camera, independent motion on the
      receiving rig, and continuous frame counts. Select Both on the camera
      for this OSC check. Repeat with a second camera observing the same hand
      and verify that camera aliases remain distinct.

- [ ] Real service connection is acknowledged only after the connected server
      reports an accepted Hyperion version; record both client and service values.
- [ ] One camera appears with stable serial/alias and separate present/subscribed/
      fresh-frame/hand-visible facts. Compare with Ultraleap's own tools.
- [ ] Move left and right hands separately; confirm raw palm/wrist/fingertip values,
      lowercase handedness and native frame/hand IDs where surfaced in diagnostics.
- [ ] Remove hands. Observe fresh empty frames without a false device failure;
      confirm the one-time legacy loss output at the actual receiver.
- [ ] Reacquire a hand and record ID changes; no previous person's filtered pose or
      gesture latch should leak into a different known native hand ID.
- [ ] Save a profile, restart the engine, reopen the controller and verify aliases,
      selected channels, hand assignment and destination are restored.

## Gate B — two independent real cameras

- [ ] Attach both; record distinct serial/alias mapping and independent progressing
      tracking counters. Repeat with reversed attachment order after persistence.
- [ ] Observe both devices concurrently in OSC In DAT/CHOP. One camera's hand must
      not overwrite the other's device prefix. No fusion/handoff is expected.
- [ ] Disable A's **output routing**, verify its frames remain independently healthy
      and B's output continues, then re-enable A. This control does not power off
      USB or promise upstream SDK unsubscription.
- [ ] Change A's left/right/both filter and verify actual receiver channels and
      persistence; B's selected route must remain unchanged.
- [ ] Close, reopen and kill/restart the GUI while the engine runs. Verify real
      TouchDesigner output continues and the new GUI reads engine truth.

## Gate C — loss, stale streams and recovery

- [ ] Unplug A while B runs: A becomes invalid/disconnected, default pose channels
      clear and B keeps processing/sending. Record timestamps and counters.
- [ ] Replug A: the original serial alias returns, handles/subscriptions are fresh
      and frames/OSC resume without restarting the whole engine. Repeat for B.
- [ ] Use the supported manual restart action on A; confirm a real resubscription
      and new frame progression while B continues. Do not mistake command ack for
      recovered fresh tracking.
- [ ] Restart Hyperion service in a controlled test environment, then observe full
      cleanup, re-enumeration and resubscription. Record actual detection/recovery
      delays and SDK results. No automatic OS service restart is implemented.
- [ ] Measure time to first frame after each successful subscription. The runtime
      allows three seconds for startup and detects a 500 ms interruption in an
      established stream. The 6.2 SDK documents that service-lost
      notification can take up to five seconds without another service transaction.
- [ ] Observe capped retries under unavailable service/device conditions; no tight
      retry/log loop. Pending service acknowledgement has a three-second deadline.
- [ ] If reproducible, exercise USB sleep/power management and CPU load; collect
      frame age, drops, processing/output counters and per-camera isolation.
- [ ] Reject an invalid OSC port without changing applied settings. Correct a valid
      wrong destination and verify reception resumes; test manual output reset.
- [ ] Restart TouchDesigner alone. Verify sending continues and the receiver can
      reattach. UDP may still report OS success with no listener; never infer TD
      receipt or a guaranteed send error from listener absence.
- [ ] Export redacted diagnostics before recovery if the reported silent stall
      occurs. Record which pipeline boundary stopped first and what restored it.

## Gate D — current global processing and OSC controls

The implemented processing parameters are **global**, with separate filter state
per device/hand/channel. Per-device routing/hand filters and the global channel
selectors are implemented. Per-channel processing overrides are not implemented.

- [ ] With default raw bypass, verify legacy scalar float addresses, units in mm,
      wrist/tip validity omissions, active port and documented loss quirks against
      the golden fixtures. Optional orientation/velocity/normal are not guaranteed
      to clear on loss; zero is not explicit validity.
- [ ] Toggle every supported channel family and observe actual TD packets: palm,
      wrist, five fingertips, pinch/grab, orientation, velocity, normal, visible
      time and finger extension. Verify Applied/Rejected state matches engine ack.
- [ ] Switch **global** raw bypass off; test time-based smoothing and dead-zone,
      scale, X/Y/Z rotation, offset and selected mm/m units on real positions.
      Preserve default raw-mm behavior when bypass is on; no hidden filter applies.
- [ ] Verify camera/hand states remain independent despite global parameters.
      Remove/reacquire a hand; inspect known-ID reset and absence of old-pose blends.
- [ ] Enable optional pinch/grab gesture output and test enter/exit hysteresis.
      Verify native pinchStrength/grabStrength remain native strengths and new
      pinchActive/grabActive messages reflect only the explicit gesture option.
- [ ] Change output enable and rate. Verify actual packets and loss clearing; do
      not label the scalar packet rate as complete-hand frame rate.
- [ ] Save/load a named profile, restart engine and reopen GUI; compare actual
      output, active settings, profile save status and backup behavior.

## Gate E — endurance and deployment

- [ ] Run an initial one-hour two-camera soak, then an eight-hour deployment soak
      when available; record exact duration, memory/handle/log growth, CPU, frame
      age/drop/retry counters and sender errors. These are targets, not prior passes.
- [ ] Repeat at least ten unplug/replug and service-restart cycles; report every
      failed cycle and recovery action, not only the successful final run.
- [ ] Leave the UI closed for most of the soak; open briefly without disturbing
      capture/output. Verify clean shutdown/relaunch has no leaked/duplicate streams.
- [ ] Validate the extracted portable package on a Windows PC without developer
      Qt paths; separately confirm installed Hyperion availability and real tracking.
- [ ] Record cold boot, login/session, sleep and optional autostart behavior relevant
      to the installation. Ordinary user-session execution is the intended model.

## Full hand skeleton

- [ ] In an OSC In DAT, receive a physical left and right hand, including a fully tracked forearm and the four segment slots of each digit. Verify the thumb metacarpal has coincident endpoints.
- [ ] Calibrate an open hand, curl the index, oppose the thumb and roll the wrist independently. Remove and reacquire the hand and check the rig handles validity and identity changes.
- [ ] Repeat on each camera and measure receiver frame time in the target scene.

## Future optional checks — not acceptance claims for this build

Per-channel raw/processed overrides, advanced calibration scope,
bundled/MTU-split packets, recorded replay and automatic output socket
supervision need their own implementation and tests before physical acceptance.
Do not mark these as broken or passed by exercising controls that do not exist.

## Results and evidence

| Date/time with timezone | Commit/package | Test ID | Environment | Result | Evidence / defect |
|---|---|---|---|---|---|
| — | — | — | PHYSICAL WINDOWS | NOT RUN | — |

Use PASS/FAIL/BLOCKED/NOT RUN. Write detailed results in
HARDWARE_TEST_RESULTS.md (or dated reports), including real TD observations and
logs/metrics, then update CURRENT_STATE.md and SESSION_HANDOFF.md. Keep software
CI/probe results in separate rows. No physical checkbox is passed by compilation,
SDK-header injection, DLL probing or simulated frames.
