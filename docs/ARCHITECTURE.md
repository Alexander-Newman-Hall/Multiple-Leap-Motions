# Architecture — Hand Bridge 2.0

**Status:** Native controller/runtime plus an optional real LeapC adapter implemented;
physical hardware acceptance remains open. Windows Release packaging is a separate
deployment gate, documented in WINDOWS_RELEASE.md and CURRENT_STATE.md.

## Implemented boundary (2026-11-15)

`handbridge-core` compiles the preserved raw-mm processor, aliases, legacy config,
sorter and OSC sender without SDK/UI dependencies. `handbridge-runtime` owns
`ITrackingSource`, deterministic `MockTrackingSource`, and `EngineRuntime`.
`handbridge-tracking` selects an explicit mock or the optional SDK-backed source.
`handbridge-engine` defaults to real tracking when SDK-enabled and reuses the
preserved production processor/oscpack serializer. `--simulate` remains explicit;
SDK-off real startup fails clearly. The default console host is Qt-free. The optional
QtCore/Network IPC host places the same runtime on a separate worker; GUI rendering
libraries and objects do not enter either engine path.

The initial runtime has a single owner thread. Nonblocking source polling emits
owned events with serial, source frame ID, source timestamp (inside FrameData),
and local monotonic arrival milliseconds. A latest-frame mailbox per serial
bounds pending work; overwrites count as drops. Duplicate/older sequences and
already stale samples do not refresh health. Up to 64 distinct device records
bound transient inventory; persistent config capacity is separate.

Per-device state records presence, subscription, fresh frames, visible hands,
processed/input/drop counts, retries and transition reasons. Empty frames are
healthy. Stale/device/service loss submits one empty legacy frame; a disappearing
service clears device presence and requires rediscovery. Device recovery is
independent with capped backoff. Service recovery uses the source start/stop seam;
it is simulated, not an assumed LeapC implementation. No cross-camera matching.

OSC uses a nonblocking checked native socket plus retained oscpack serialization.
Output failures count separately and do not stop processing. The default host reports transitions/final JSON; the optional IPC host also
publishes sampled telemetry to the native controller. Actual GUI attach/close/
kill/reopen has been tested against continuing mock-source UDP output.

Still proposed below: per-channel processing overrides,
explicit OSCv2 validity, full 3D/all-joint preview and production packaging. The original
ImGui path remains separate and retains its known lifecycle limitations.

## LeapC adapter and DLL ownership

`LeapCApi` obtains every type/signature/calling convention from the actual official
SDK header. The production adapter owns one multi-device-aware connection and
SDK-id/serial/handle/subscription records on its runtime owner thread. It waits for
the SDK Connection event before enumeration; poll uses timeout0 and an eight-event
budget. A three-second handshake deadline and capped retries prevent busy loops.
Known stale streams restart only their device; device/service loss releases handles
and invalidates output through the existing supervisor. Enumeration refreshes
transient references without closing healthy cameras. SDK pointers are copied
before the next poll; native hand IDs remain local to one device.

`LoadedLeapCSource` owns the DLL before the adapter, so reverse destruction cleans
the SDK handles before FreeLibrary. The loader resolves twelve required exports
from an absolute selected path using safe Windows search flags; no bare DLL is
searched on PATH or in the working directory. `--check-leapc` verifies load/exports/
version without opening the service or writing configuration. The twelve required
exports include the real SDK `LeapGetVersion`: client and server-library versions
must each be6.2.0 or a newer6.x. Service health waits for the server version check
after an actual connection event. Engine diagnostics
publish actual library identity, attempts, poll errors and retained errors; exports
redact the path. None of this establishes physical tracking/service compatibility.

Known native hand-ID changes reset only that hand's positional and gesture state,
including raw-bypass gesture handling. Unknown IDs preserve legacy/mock behavior.
Legacy OSC addresses/types/defaults and schema2 remain unchanged.

## Context and process split

```text
 [Leap Motion Controller 2 A]      [Leap Motion Controller 2 B]
              \                           /
               \       Ultraleap         /
               [ Tracking Service / LeapC ]
                              |
                   LeapC adapter / poller
                    (device-aware; RAII)
                              |
                   per-device frame envelopes
                              |
                   bounded latest-frame queues
                              |
                      tracking processor
             (select -> transform -> filters -> route)
                              |
                  frame/channel output model
                              |
                  OSC serializer / UDP sender
                              |
                    127.0.0.1:PORT
                              |
                   TouchDesigner OSC In

      [handbridge-engine.exe] = everything above, no GUI needed
                 |
         local-only control IPC
                 |
      [handbridge-control.exe]
       native Qt 6 graphical UI
       diagnostics / controls / presets / preview
```

**Hard boundary:** no GUI object or event loop may own the LeapC connection, frame processing, or OSC transport. IPC carries configuration commands, low-rate summaries and optional preview copies; it does not carry full-rate frames used by OSC.

## Suggested targets
- `handbridge-domain`: immutable value types for frame, device, hand, joint, timestamps and units; serializers for versioned config/IPC.
- `handbridge-tracking`: provider interface, LeapC adapter, simulator/replayer, connection supervisor; native SDK headers isolated here.
- `handbridge-processing`: per-device selector and independent module chain; pure functions where possible; supports deterministic tests.
- `handbridge-output`: OSC legacy serializer, optional v2 serializer and UDP sender, bounded outgoing work.
- `handbridge-runtime`: owns lifecycle/config/state/health/queues; executable entry point and headless CLI.
- `handbridge-ipc`: control commands, acknowledgements and sampled telemetry, local-only transport.
- `handbridge-ui`: native Qt 6 Qt Quick/QML presentation and C++ model/controller. GUI connects to running engine and does not auto-stop it on exit.

Prefer simple modules close to the existing code rather than inventing a large plugin ecosystem before tests pass. Backend may use Qt Core/Network for local IPC, but **never** require Qt GUI/Quick in headless process.

## Concurrency and lifetime
- Exactly one owner for each LeapC connection/device handle. Open, subscribe, unsubscribe and close on designated tracking owner thread, following installed SDK contract. Never use device handles after disconnect.
- Hardware polling is a dedicated worker. It stamps frames with source ID and arrival monotonic time, then passes owned copies/references through bounded queues.
- Processing is independent of UI. On backlog, prefer the latest valid frame to old stale frames; count drops. Do not sleep/block in LeapC callbacks for logging, networking or GUI calls.
- OSC sender is isolated; socket errors reported without blocking camera polling. Configuration snapshots are immutable and swapped atomically at a frame boundary or version-controlled transaction.
- GUI telemetry is sampled at low rate. An IPC client cannot keep the engine alive, block it, or induce unbounded memory consumption.
- Graceful shutdown: stop accepting config changes, flush/mark validity as appropriate, stop workers, unsubscribe/close device handles, close LeapC connection, close socket, persist clean settings; avoid deadlocks.

## Health state and self-healing

Track these distinct facts (never collapse them into one green indicator):
1. `service_connected`: valid LeapC service connection.
2. `device_present`: device enumerated/USB present.
3. `device_subscribed`: event subscription acknowledged.
4. `frames_fresh`: tracking frame received recently, INCLUDING frames with zero hands.
5. `hands_visible`: zero/one/multiple hands observed in latest valid frame.
6. `processor_running`: frames consumed, counted and timestamped.
7. `osc_socket_ready`: transport available; `osc_send_success` describes OS send result only.
8. `touchdesigner_receiving`: only if explicitly verified by external TD receiver or optional loopback test, never inferred from UDP send.

Proposed per-device lifecycle states: DISABLED, CONNECTING, SUBSCRIBED, STREAMING, DEGRADED, RECONNECTING, DISCONNECTED. Use transition reasons (`SERVICE_LOST`, `DEVICE_LOST`, `SUBSCRIBE_FAILED`, `FRAME_STALE`, `RECOVERED`, etc.) and timestamps.

- Service disconnect: invalidate relevant handles/subscriptions, clean up, bounded retry with capped backoff, re-enumerate and restore serial-based configuration after reconnection.
- Device lost: clear its live tracking validity and output state, close only its resources, retain configured alias, continue other devices.
- Frame stale while connection appears valid: count/alert, diagnose before cycling; compare with service/device events; controlled single-device recovery if supported. Avoid restart loops when no hands are in view.
- OSC destination invalid: reject config; send error: mark output degraded and attempt sender recovery as appropriate, without interrupting tracking.
- Recovery events should be rate-limited in logs, but never silently hidden.

## Control API (proposed JSON messages framed over local IPC)

The exact schema and version will be fixed by tests. Start with length-prefixed UTF-8 JSON and a maximum message size. Use request IDs and config revision numbers.

Commands:
- `get_status`, `get_config`, `list_devices`, `subscribe_telemetry`
- `patch_config` with expected revision → response applied/rejected with validation errors and new revision
- `save_config`, `load_profile`, `save_profile`, `validate_config`
- `restart_device` (explicit, logged), `reset_output` (explicit)

Telemetry sampled rather than full-rate: device state, FPS, age, queue depth/drop count, OSC send/error counts, selected skeleton preview, config revision and last error.

Use `QLocalServer`/`QLocalSocket` (Qt Network) or an alternative documented in `docs/DECISIONS.md`. Limit endpoint to same user where supported; no remote control by default. Disallow simultaneous conflicting controllers or use revision checks to prevent lost updates. Handle GUI reconnect by fetching entire authoritative state.

## Extension seams
- Optional fixed device-to-world transformations for a rigid 3D-printed camera frame.
- Future providers, handoff, fusion, shared memory only through explicit ADR and after base product passes reliability tests.
- Transport adapters can be added later, but OSC remains the release-default output.

## Implemented native controller boundary

With HANDBRIDGE_BUILD_CONTROL=ON, handbridge-engine uses QtCore/Network for local
control only. A dedicated owned std::thread owns source/runtime/processing/sender;
bounded32-command work and mutex-protected immutable shared snapshots connect it
to the IPC thread. Stop sets a flag under the command mutex, wakes and joins
outside locks; startup exceptions join before constructor unwind. Snapshot JSON
is built outside its separate pointer-exchange mutex. See ADR-017.
QLocalServer parsing, sampled telemetry and QSaveFile profile I/O do not execute
on the tracking worker. The host links neither QtGui nor QtQuick. The separate
handbridge-control process owns the QtQuick scene and C++ draft/ack model; it can
exit or crash while the worker continues. Default Qt-free host is still built
when the option is off.

Settings are validated plain C++ values; the runtime applies a whole candidate at
a frame boundary. Socket destination readiness is checked before committing.
Raw observations and processed preview frames remain separate. Source identity
still lacks native hand IDs until the real adapter is built. User-approved GUI
work changes milestone order; it does not validate USB/Hyperion behavior.
