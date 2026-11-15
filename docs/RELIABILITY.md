# Reliability, reconnection, and silent-output-stall diagnosis

**Background:** The original owner reports a real-world failure: the program continues running, but tracking data stops reaching TouchDesigner. The exact cause is UNKNOWN. Treat this as the highest-priority investigation, not as proven to be USB, driver, threads, sender, or service.

## Diagnostic principle

Instrument boundaries independently. The system can be operational at one boundary and broken at the next:

1. Ultraleap Tracking Service connected (or error/reconnecting)
2. Per-device USB/reference discovered and opened
3. Per-device LeapC event subscription active
4. `LeapPollConnection` produces tracking events
5. Frames converted/enqueued successfully (and frame IDs progressing)
6. Frames consumed/processed successfully
7. OSC messages serialized and UDP sends attempted successfully
8. TouchDesigner actually receives and interprets them (separate receiver-side observation)

**Never infer successful receipt from UDP socket validity or successful send.** Normal no-hands tracking frames must count as healthy frame activity even if no positions are being emitted.

## Lifecycle state machine (per service and each device)

Service:
`Stopped -> Starting -> Connected -> Recovering -> Connected` or `Fatal/Disabled`.

Device:
`Absent -> Discovered -> Open -> Subscribing -> Streaming -> Stale/Recovering -> Streaming`, with `Disabled`, `Removed`, `Error` explicit branches.

- Record the cause, timestamp and SDK result code on every transition.
- Classify retryable versus nonretryable failures; use bounded exponential backoff with jitter, a maximum interval and retry counters. Do not spin at 1ms in a permanent error state.
- On device removal: mark invalid immediately, unsubscribe/close if safe, clear stale hand state, re-enumerate for future reconnection. Device handles are not portable between service reconnections.
- On service loss: unwind subscriptions/handles and recreate service connection according to exact SDK lifecycle documentation, then rediscover/reopen/resubscribe devices. Single-device restoration must not silently suppress the other.
- If a stream goes stale while a device remains connected: collect evidence first (poll error? no frames? queue blocked? CPU?), attempt targeted recovery before full service reset.
- A watchdog must not classify absence of HANDS as a stall; it should examine arrival of FRAME messages independently of hand visibility. Investigate whether idle frame behavior varies by SDK/device.
- Implement a manual retry/restart command, but rate-limit it and report actual result.

## Timer and status recommendations (tunable, not asserted SDK requirements)

Suggested initial stale threshold: 500 ms of no frame events, adjustable and calibrated against measured device behavior. Slow warning at 250 ms can help during tests. Maximum reconnect backoff e.g. 10 s after exponential retries; document actual values chosen. Do not hard-code without validation. Use monotonic local clock for elapsed durations; retain source timestamps and distinguish clock domains.

Record `last_frame_received_at_monotonic`, `last_valid_hand_at_monotonic`, `last_polled_at`, `last_output_attempt`, and `last_output_success`. The last two describe sender calls only.

## Concurrency rules

- LeapC poll thread does the minimum callback work: safely copy data and enqueue; never write logging per scalar value and never invoke UI rendering.
- Check ownership: the original `SpscQueue` is **single-producer/single-consumer**. If future architecture adds multiple poll threads, either use per-device SPSC queues or replace it with an appropriately designed MPSC queue. Do not assume SPSC supports multiple producers.
- Queue metrics include attempted/enqueued/dropped/consumed counts, high-water mark and oldest frame age.
- On overflow: count drops unconditionally; expose to GUI and logs with rate limiting; document whether newest replaces oldest or newest is discarded.
- C++ ownership must prevent use-after-free during device removal, callback dispatch and process shutdown. Use RAII for SDK handles and thread joins.
- A slow GUI, blocked file write or unreachable OSC destination cannot block the tracking poll loop.
- Bound diagnostic logs and metrics so long-running installations cannot fill the disk.

## OSC validity on hand loss

- Legacy output previously emits zeros when a hand that was present disappears. Preserve this within the compatibility profile until measured/verified.
- Add explicit validity/presence channels in opt-in v2 mode, so a real hand position `[0,0,0]` is distinguishable from no hand.
- On device loss or service restart, emit an unambiguous loss transition and clear stale processing state; no indefinite frozen pose.
- Test missing-hand transitions, device disappearance and output disable separately.

## Fault-injection matrix

| Fault | Inject in mock/CI | Require on Windows |
|---|---|---|
| No hands, frames still arrive | Yes | Yes |
| One device disconnects | Yes | Yes (physical unplug) |
| One device reconnects | Yes | Yes |
| Service connection lost | Yes | Yes (service stop/restart where safe) |
| Subscription fails | Yes | At least observe error handling |
| Poll returns non-timeout error | Yes | Error log only if encountered |
| Queue reaches capacity | Yes | Stress simulation |
| OSC target port changed | Yes | Receive on alternate TouchDesigner port |
| UI process crashes | Yes | Yes |
| App killed/restarted | Yes | Yes |
| Data processing throws/blocks | Yes | Stress test |
| Device sends duplicate/older frame ID | Yes | If reproducible |

## Bug report template

- Version, commit, build profile, Windows build and Ultraleap service/SDK version
- Device models, firmware/serial (redact when shared), USB port/controller/hub details
- Number of devices and time since startup; CPU/memory/USB power conditions
- Local timestamps and pipeline stage counters before/after incident
- `service_state`, `device_state`, `last_frame_age_ms`, `frames_in`, `queue_drops`, `frames_processed`, `osc_packets_sent`, `osc_send_errors`
- Whether TouchDesigner OSC In CHOP/DAT observed packets; check UDP destination and listener settings
- Whether hardware restart, app restart or just UI restart restored output
- Attached sanitized bounded logs/diagnostic bundle

## Acceptance condition

A simulated or physical failure must result in a visible named state within a configurable detection window, truthful metrics, controlled recovery when the fault clears, no silent permanently frozen output, and continued operation of unaffected cameras. Physical reliability remains unverified until Windows hardware tests are documented.

## Implemented mock supervisor (2026-11-15)

The new EngineRuntime, not the old AppCore, implements the first reliability slice:

- Stale threshold 500 ms since the latest accepted frame (or subscription before
  the first frame); zero hands counts as fresh. All elapsed checks use injected
  monotonic milliseconds, distinct from SDK microsecond timestamps.
- Once stale/lost, clear pending data, mark hands/freshness false, and submit one
  empty frame through the preserved legacy processor. Repeated ticks do not repeat
  loss packets. Optional legacy orientation/velocity/normal loss quirks remain.
- First targeted retry after 100 ms, failure delays 100/200/400/800/1000 ms capped
  at 1000 ms; no retry spin. Successful subscription awaits actual fresh frames
  before STREAMING. No jitter in deterministic foundation; tune on hardware later.
- Service loss clears present/subscribed state, stops the source, and retries its
  start seam with capped backoff. Only rediscovered devices become present again.
- One pending frame per device; replace old with newest, count overwrites,
  reject duplicate/regressing sequences and samples at/over stale age. This
  bounds current pending storage, not the future LeapC worker queue.
- Output attempts/success/errors count actual checked callback results. A tick
  containing failures degrades output; recovery requires sends succeeding after
  250 ms without further failures. No-hands/no sends cannot claim recovery.
  Sender errors log at most once/second even if connected UDP alternates errors
  and successes. Metrics never imply receiver acknowledgement.

Tests inject service loss, missing inventory after restart, subscription failure,
frame stalls, 100 reconnect cycles, mailbox overflow, duplicate/stale frames,
normal no-hands and output failure/recovery. The process test independently decodes
UDP during a two-camera stall. There is no real SDK/service/USB recovery yet.
Automatic output socket recreation, manual restart commands, raw SDK result-code
telemetry, measured FPS/latency, threaded poller supervision and a replay-file CLI
are remaining work. A blocked provider poll cannot be interrupted by this
single-thread foundation: the source contract requires bounded nonblocking poll;
Phase 2 must enforce the real LeapC poll timeout/worker lifecycle.
