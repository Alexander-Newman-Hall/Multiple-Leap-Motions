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
