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
