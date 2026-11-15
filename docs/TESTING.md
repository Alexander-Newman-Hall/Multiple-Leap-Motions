# Test engineering — unit, mock, integration and physical validation

## Test levels

**Unit tests (no SDK):** serial alias mapping, config migrations, selector semantics, time-based smoothing/processor reset, coordinate and quaternion transforms, OSC serializer golden packets, packet size limits, presence flags and invalid inputs.

**Mock integration tests (no SDK):** two independently simulated devices; output to a loopback UDP receiver; queue/processing pipeline; native control IPC and config apply/acknowledgment; frame stall, reconnect, UI kill/restart without stopping engine.

**SDK integration tests (Windows with SDK):** adapter initialization/open/subscription and failure handling, if service present. A compiled adapter is not a physical test.

**Physical hardware tests (Windows with cameras):** real discovery/streams/hotplug/service restart, two-device independence, long-run health, TouchDesigner receiver, visual UI validation. See `docs/HARDWARE_TEST_PLAN.md`.

## Deterministic frame fixtures

- Generate synthetic frames with fixed device IDs, serials, frame counters, timestamp domains and known palm/fingertip vectors.
- Include left/right and multiple-hands, empty frames, handedness swaps, lost/reacquired IDs, duplicate frames and invalid joints.
- Model time passage with injected monotonic test clock rather than `sleep()` in most tests.
- Store a few sanitized JSON/CBOR frame fixtures or C++ literals. No sensitive camera imagery is needed.
- Use a `MockTrackingSource` that shares the same interface as the LeapC adapter, not a separate demo-only code path.

## OSC golden tests

- Capture existing exact addresses, lowercased handedness, float argument count/encoding, units, order where meaningful and selected toggles before refactor.
- Decode outgoing OSC packets with an independent parser/test receiver. Compare numerical tolerances appropriately; do not compare unordered bundle byte order unless protocol guarantees it.
- Test port changes, destination mismatch, missing hands, disabled channels and both cameras.
- Test optional bundling with splitting at configured safe UDP payload size; do not silently switch the legacy default.

## Concurrency/robustness

- Repeated start/stop and disconnect/reconnect cycles with sanitizers where available.
- Prove UI process death does not stop engine or OSC.
- Assert queue full count is incremented, not hidden behind logging level.
- Verify no deadlocks on configuration update/reconnect and no use-after-free of LeapC handles.
- Measure events/seconds and end-to-end latency only with explicitly described clocks; avoid guessed benchmarks.

## Test reporting

Every test report must include date, commit SHA, OS/toolchain/SDK versions (if used), command, pass/fail/skip counts and links to log artifacts. `SKIPPED: no hardware` is not equivalent to success. Update current state and session handoff docs after test runs.

## Executable baseline (2026-11-15)

Default CTest discovers 40 cases: 10 config, 7 aliases, 1 retained sorter,
8 actual scalar OSC/UDP, 13 runtime/mock, and 1 Python process/UDP integration.
Run the Debug/Release or Windows presets in root README. Python is an independent
receiver/test harness only. The tests do not instantiate a fake hardware binary
or count obsolete original tests as passing. Detailed exclusions/compile evidence
and sanitizer commands are in [BUILD_AUDIT](reports/BUILD_AUDIT.md).

Runtime tests inject monotonic time rather than sleeping: stale/no-hands,
subscription failure and capped backoff, duplicate/old frames, mailbox overwrites,
service re-enumeration inventory, 100 reattachments, no-send output-health rules
and start/stop. The process test binds an ephemeral localhost receiver before
launching the actual headless executable, parses padded OSC/network float bytes,
checks B sends during A loss/recovery, actual config aliases/channels and CLI
rejection. Physical Hyperion, USB, TouchDesigner and GUI lifecycle checks remain
NOT RUN. Sanitizers cover project code/tests, not third-party dependencies.

The actual broadcast-permission kernel regression is POSIX-only: Windows CI's UDP
provider accepts that send, so the case skips on Windows. Windows readiness/closed
socket failures and all decoded UDP/runtime/config tests still execute. Report
39 passed + 1 platform-specific skip instead of claiming 40 Windows passes.

## Native controller acceptance (implemented)

The default Qt-free suite now has55 CTest entries. Optional control-debug /
windows-control adds three QtTest bundles,58 CTest entries total: ControllerModel
(9 scenarios+init/cleanup), actual native QML typing/clicks affecting UDP, and
ControlIntegration (10 process/persistence behaviors+init/cleanup). UI tests use
software/offscreen native Qt; they are not HTML/browser screenshots.

Settings tests exercise real scalar output for channel/mute/hand/alias/rate,
deadzone/time smoothing, transforms/units and opt-in hysteresis. Default legacy
golden fixtures remain unchanged. Controller tests withhold acknowledgements and
inject delayed/stale responses to verify editor truth. QML tests type a new port
and click Apply directly, then observe actual packets on the new receiver and
transmission stopping/resuming from the GUI switch. Integration closes/kills/
reopens real GUI processes while engine PID and changed OSC values continue.

Protocol tests cover20 batched frames, malformed/version/oversized input, unread
client flood, duplicate engine (even alternate state directory), CAS, diagnostics
redaction and actual profile/current/backup files. A production ConfigurationStore
regression delivers an older save after a newer one and checks actual bytes stay
unchanged. Initial discovery after restore advances revision and is not Saved.

Final cloud58/58 PASS, no skips. A separate ASan/UBSan run passes56 entries with
leak checking, covering project core/process/client model while excluding the two
GUI bundles and third-party Qt instrumentation. Exact commands/results and final
CI distinction: CURRENT_STATE, SESSION_HANDOFF and reports/GUI_BUILD.md. Windows
retains the explicitly skipped POSIX-only kernel-denial case; no skip is hardware
verification. Real SDK/USB/Hyperion/TouchDesigner/soak remain UNTESTED ON HARDWARE.


Final source ebb3bd9 passes native Windows and Ubuntu CI on both push and PR.
Windows adds three until-fail repetitions of the process bundle; all passed on
both events. The applied-preview QML regression verifies unacknowledged units do
not change live projection, and raw bypass remains mm even with inactive meter
settings. Detailed retained logs/results: CURRENT_STATE and GUI_BUILD.
