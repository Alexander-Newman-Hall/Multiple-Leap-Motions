# Reliability, recovery, and silent-output-stall diagnosis

**Current scope:** the headless runtime now has a real LeapC adapter compiled
against the official Hyperion 6.2 SDK, alongside the deterministic simulator.
Injected-API tests execute production adapter code with genuine SDK declarations;
they do not execute the SDK binary or prove camera behavior. Physical Windows,
USB, service-recovery and TouchDesigner checks remain **UNTESTED ON HARDWARE**.
The original reported field stall's root cause remains unknown.

## Distinguish each boundary

Track service connection, per-device discovery/open/subscription, fresh tracking
frames, visible hands, processing activity, and OSC socket/send results separately.
A connected service does not prove a camera is subscribed. A successful subscribe
call does not prove frames are fresh. Fresh frames with no hands are healthy.
A successful OS UDP send does not prove TouchDesigner received anything.

The Qt controller reads engine telemetry through local IPC. Tracking and OSC run
on their own engine worker; rendering, client disconnects and profile disk writes
do not own that worker. Closing or crashing the GUI must not stop the engine.
The native GUI shows real and simulated provider modes separately; the simulator
is selected explicitly, never substituted for an unavailable hardware provider.

## Verified SDK contract and version policy

The current target is Hyperion **6.2.0 or newer within 6.x**. The dynamic loader
checks the real client library using `LeapGetVersion(ClientLibrary)`. After an
actual SDK connection event, the tracking owner queries `ServerLibrary` and checks
its version before emitting `ServiceConnected` or enumerating devices. Missing,
unknown, older, or different-major versions cannot become a healthy service state.
Client DLL metadata, API client version and connected service version are distinct
facts. See [SDK setup](LEAPC_SETUP.md) and [SDK audit](reports/LEAPC_SDK_AUDIT.md).

`LeapOpenConnection` is asynchronous; enumeration waits for the SDK connection
event. Multi-device awareness is requested at creation, each device is explicitly
subscribed, and tracking events are routed by `message.device_id`. Device events
carry their ID inside `device_event.device.id`. SDK event pointers expire on the
next poll/close, so all hand observations are copied before polling again.

The exact 6.2 header says a service-lost event can take up to **five seconds** of
polling without another service transaction. The application frame watchdog is
separate: stale data must not stay live while waiting for that notification.
Do not interpret the 500 ms frame threshold as an SDK service-loss guarantee.

## Implemented lifecycle and limits

The source owns the connection and opened handles; its start, poll and restart
operations run on one owner thread. Stop/cleanup must not overlap SDK calls.
Subscriptions and owned device handles are released before closing/destroying the
connection. Borrowed SDK device references are never treated as owned handles.

- `LeapPollConnection` timeout is **0 ms** (documented immediate return), with at
  most **eight SDK messages per source poll**. A connection that never acknowledges
  the service is torn down after a **three-second handshake deadline** and retried
  through the runtime's bounded schedule.
- Discovery uses two-pass list and serial queries with bounded storage. The source
  retains initially failed opens/serial queries for capped retries, and refreshes
  enumeration every **1000 ms**. Failed subscriptions remain visibly different
  from successful ones; the runtime supervises identified-device retries.
- Device loss removes only that device's resources/output state. Re-enumeration
  restores serial identity after transient SDK IDs change. Duplicate device events
  do not create duplicate streams; duplicate serials retain one owner. Service
  loss releases all handles and requires discovery in the new service session.
- Manual and stale recovery of a subscribed device actually unsubscribe/close and
  reopen/resubscribe that device. The unaffected camera is not restarted.
- Runtime stale detection is **500 ms** since the latest accepted frame, or since
  subscription before the first frame. Empty-hand frames refresh liveness.
  First targeted retry follows after **100 ms**; failure delays are
  **100/200/400/800/1000 ms**, capped at 1000 ms. The deterministic schedule has no
  jitter. Successful subscription still waits for fresh frames before STREAMING.
- One pending frame per serial replaces older pending work and counts overwrites.
  Duplicate/regressing frame IDs and already stale samples do not refresh health.
  Source `tracking_frame_id` and SDK microsecond timestamp are preserved separately
  from local monotonic arrival milliseconds. Native hand IDs are retained.
- A change between known native hand IDs resets only that hand's positional
  smoothing/dead-zone and gesture hysteresis, even without an empty frame. Other
  hands/devices keep their own state. Unknown legacy/simulated IDs keep the prior
  behavior. Loss, restart and processing-setting changes also reset filter state.

These bounds are application policy, not measured USB/service recovery latency or
hard real-time guarantees. Other SDK calls and Windows scheduling still require
physical measurement. No system tracking service is restarted automatically.

## Output and stale-pose behavior

Stale/device/service loss clears pending data and freshness/hand visibility, then
submits one empty frame through the retained legacy processor. Repeated stale
polls do not repeat the loss transition. Default legacy pose channels zero once;
optional orientation/velocity/normal keep their historical loss quirks. Zero is
not a general validity signal and explicit OSC v2 validity is not implemented.
See [OSC contract](OSC_PROTOCOL.md).

The checked nonblocking sender counts actual OS attempts/successes/bytes/errors.
Output health degrades on failure; recovery requires another successful send after
250 ms without further errors. Silence alone cannot claim recovery. Error logging
is bounded, including connected-UDP platforms that alternate errors and successes.
Manual output reset recreates the socket. Automatic socket recreation/backoff and
receiver acknowledgements are not implemented; correct a bad destination and
verify reception independently. Device enabled/disabled currently controls output
routing, not physical USB power or upstream SDK streaming.

## Evidence and remaining acceptance

The current genuine-SDK-type suite has **21 passing injected adapter tests**:
connection/subscription ordering, owned frame copies, empty frames, device/service
loss, failed opens/info/subscriptions, duplicates/ref reuse, bounded polling,
handshake failure, real targeted restart, runtime loss/stale integration and
server-version rejection/acceptance. Full-suite results are in CURRENT_STATE.

| Failure or check | Automated evidence | Physical Windows result |
|---|---|---|
| Empty hands, fresh frames | Simulator and injected SDK events | NOT RUN |
| One device loss/reconnect; other device continues | Production adapter/runtime injection | NOT RUN |
| Service loss, fresh references and aliases | Production adapter/runtime injection | NOT RUN |
| Failed open/serial/subscription and non-timeout poll errors | Injected SDK result codes | NOT RUN |
| Mailbox overwrite and stale/duplicate frames | Deterministic runtime tests | NOT RUN |
| GUI close/kill/reopen, real loopback OSC continues | Separate-process integration | NOT RUN with cameras |
| Config/routing/processing/profile changes affect UDP | Actual simulated-process receiver tests | NOT RUN in TouchDesigner |
| Client DLL exports/version; no service installed | Windows software/probe gate; see current results | Not a camera test |
| Blocked SDK call, hardware soak/CPU/latency | Not established by injected tests | NOT RUN |

Recorded replay, automatic output supervision, complete processing-stage stall
supervision and physical soak measurements remain future work. A successful
Windows compile, DLL probe, packaged-app smoke or simulated frame run cannot pass
these physical gates.

## Incident capture

Record commit/package hash, Windows/Qt versions, client/API/service versions,
provider mode, controller models, redacted serials, USB topology, active profile,
OSC destination and TouchDesigner build. Capture per-device frame IDs/ages,
subscription/recovery reasons, input/processed/drop counts and reconnect attempts;
output attempts/errors and actual receiver observations. Export bounded redacted
logs before targeted recovery. Record which action restored output. Distinguish
confirmed source behavior from hypotheses about USB, Hyperion, scheduling or TD.

Use [the physical acceptance plan](HARDWARE_TEST_PLAN.md) and record results in
HARDWARE_TEST_RESULTS.md. No physical reliability milestone is complete until the
corresponding evidence has actually been collected.
