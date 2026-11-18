# Local native control protocol v1

Implemented by the optional Qt Core/Network engine and separate Qt Quick client.
No HTTP/WebSocket server, remote endpoint or account is involved. The protocol
version remains 1; provider diagnostics below are additive fields.

## Transport and ownership

QLocalServer/QLocalSocket requests UserAccessOption. On macOS the socket lives
in `/private/tmp/handbridge-<effective UID>/`. Both peers require a real
directory owned by that UID with mode 0700. The filename is the first 32 hex
characters of SHA-256 of the UTF-8 endpoint token. The default endpoint is
`handbridge-<first16 SHA256(homePath)>-v1`; explicit endpoint tokens use 1–80 ASCII
letters/digits/dot/underscore/hyphen. An endpoint lock in private per-user appdata
is independent of `--state-dir`; a second engine cannot replace an active endpoint.
Do not infer physical Windows/USB behavior from local socket tests.

Each message is a four-byte unsigned big-endian payload length followed by a UTF-8
JSON object. Maximum payload is 64 KiB; zero/oversize/invalid JSON disconnects the
client. Read/write buffers are bounded at 256 KiB; at most 8 clients and 32 queued
engine commands. Parse at most 16 frames per event-loop turn, then schedule a drain
for complete buffered frames. A slow/unread client is disconnected; it cannot block
the source/OSC worker. Telemetry is sampled at 10 Hz with bounded 128-line logs.

The source/runtime/sender have one independent C++ worker. IPC parsing and disk
persistence run on the Qt Core thread. The GUI sends commands and renders copies;
it never owns tracking. **Start engine** asynchronously checks the installed LeapC
client with `--check-leapc`, then launches the real engine detached on success.
**Start simulator** and controller `--demo` explicitly select mock cameras. Closing,
killing or restarting the controller leaves an already running engine alive; an
owned preflight process is not the tracking engine.

## Requests and responses

```json
{"version":1,"id":12,"command":"apply_config","expected_revision":3,"config":{"schema_version":2}}
```

The abbreviated config illustrates framing only; an apply needs the complete
validated configuration from `get_state`. Nonnegative integer ids and revision
numbers are bounded by exact JSON integer precision (2^53−1). Unsupported
versions/commands reject. Apply/load require an expected revision; GUI saves
include it too.

| Command | Fields | Behavior |
|---|---|---|
| `get_state` | — | Full canonical config/state/revision |
| `apply_config` | `expected_revision`, `config` | Strict validation, new target socket ready before commit, acknowledged at worker boundary |
| `list_profiles` | — | Saved names |
| `save_profile` | `name`, optional `expected_revision` | Save applied snapshot and startup restore atomically with backups |
| `load_profile` | `name`, `expected_revision` | Validate/apply saved snapshot; persist startup restore |
| `restart_device` | `serial` | Invalidate/restart one source, other devices continue |
| `reset_output` | — | Recreate output socket, tracking continues |
| `export_diagnostics` | — | Write bounded JSON with serials and library path redacted in the private diagnostics directory; return path |

Responses include `version`, `id`, `ok`, optional `error`, `revision`,
`saved_revision`, `config`, `state`, `logs`, `profiles`; export also returns `path`.
A rejected apply retains the previous snapshot. A load persistence failure can
follow an applied snapshot: the error explicitly says applied-but-unsaved and
returns actual state. The client must not infer state solely from a success flag.
Saved revisions never regress because of delayed callbacks. Device discovery can
add canonical aliases and advance the revision; those changes are unsaved until
persisted.

Unsolicited telemetry has `event:"state"`, no request id, and the same canonical
state payload. State includes a devices array with serial/alias, presence,
subscription, freshness, hand visibility, frame age/FPS/counts/retries and observed
raw/processed point previews. It also includes cumulative native packet/error/byte
counters, destination/readiness, measured packet rate and socket generation.
OS send success never indicates receiver acknowledgement.

## Provider and version diagnostics

The same controller attaches to either provider; selecting real mode does not
change the OSC protocol or configuration defaults.

| State field | Meaning |
|---|---|
| `source` | `"leapc"` for the real adapter, `"simulated"` for explicit mock mode |
| `simulated` | Boolean matching the selected provider; never inferred from frame count |
| `hardware_verified` | Remains `false`; compilation, DLL loading and simulated frames are not physical acceptance |
| `service_connected` | Actual source acknowledgement; the LeapC source also checks the service API version before declaring connected |
| `tracking.last_error` | Retained source diagnostic history; empty when none is reported. It is separate from current service/device/freshness state |
| `tracking.library_path` | Actual selected LeapC library path; empty for the mock provider |
| `tracking.library_version` | Windows DLL file-version resource, such as `6.2.0.0`; `"unknown"` if unavailable. This is not the API compatibility check |
| `tracking.service_version` | Version reported by `LeapGetVersion(ServerLibrary)`, such as `6.2.0`; empty before a result or after a new connection attempt resets it |
| `tracking.connection_attempts` | Actual source client-connection creation attempts |
| `tracking.poll_errors` | Counted source polling failures; ordinary no-frame timeouts are not failures |

These fields are also included in the hosts' final CLI summary. Mock diagnostics
have no installed-library/service identity and do not fabricate SDK errors or
connection counts. Error transitions enter the bounded log; repeated identical
errors do not produce a line per frame. Exported diagnostics replace
`tracking.library_path` with `<redacted>` and redact known device serials.
Local status/CLI output may show the actual library path for troubleshooting.

The client compatibility gate uses `LeapGetVersion(ClientLibrary)`, independently
of the DLL file-version resource. Both client and tracking service must report
major **6** and minor **2 or newer**, with a valid nonnegative patch: Hyperion
**6.2.0+ within 6.x**. Older clients/services and other major versions are rejected;
there is no silent fallback to simulation. The client version is checked by the
loader; the service version is checked after its actual connection event and
before healthy service state. See [LeapC setup](LEAPC_SETUP.md).

The client retains dirty edits during telemetry, rejects stale CAS, ignores older
responses in a live connection, and refreshes after reconnect. A new connection
may have lower revisions after an engine restart. Saved/Applied/Editing/Rejected
are distinct; field appearance never substitutes for an engine acknowledgement.

## Host and controller CLI

Qt engine:

```text
handbridge-engine [--simulate] [--headless] [--config file]
  [--host IPv4] [--port N] [--endpoint token] [--state-dir private-dir]
  [--scenario normal|stall] [--duration-ms N]
  [--leapc-library absolute-library-path] [--check-leapc]
```

Without `--simulate`, a build with `HANDBRIDGE_WITH_LEAPC=ON` selects the real
provider. SDK-off builds fail clearly in that mode. The real runtime loader is
Windows x64; other platforms retain header-based adapter tests and explicit
simulation. `--scenario stall` requires `--simulate`; an explicit library path
cannot be combined with simulation. `--help` and `--version` are supported.
The Qt-free host supports the same provider/OSC/duration flags, but not local IPC,
`--endpoint`/`--state-dir` or the Qt host's schema-2 profile operations.

`--check-leapc` loads the selected installed library, validates required exports
and its client API version, then prints one JSON object and exits. It does not
open a service/camera connection, start OSC, bind an IPC endpoint or write profile
state. It cannot be combined with `--simulate`. Successful output includes:

| Preflight field | Meaning |
|---|---|
| `provider` | `"leapc"` |
| `library` | Actual selected absolute library path |
| `version` | DLL file-version resource, distinct from API version |
| `client_api_version` | Actual `LeapGetVersion(ClientLibrary)` version accepted by the 6.2+ within 6.x policy |
| `hardware_verified` | `false` |

Preflight success proves neither service compatibility nor camera/USB/TouchDesigner
operation. It is safe to run before physical setup; missing/unsupported libraries
produce a nonzero exit and an actionable error. No proprietary LeapC SDK/runtime
DLL is included in the portable package; install the compatible official runtime
and tracking service separately. Qt/application deployment is separate from SDK
redistribution.

Controller: `handbridge-control [--demo] [--endpoint token]`. `--demo` starts a
simulator only if no engine is connected; closing the window does not stop it.
Automation supports `--exit-after-ms`, `--page overview|devices|processing|osc|profiles`
and `--screenshot path.png`. These render the actual native window, not a mockup.

Official references: [QLocalServer](https://doc.qt.io/qt-6/qlocalserver.html),
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html),
[Qt Quick](https://doc.qt.io/qt-6/qtquick-index.html), and the SDK evidence in
[LEAPC_SDK_AUDIT](reports/LEAPC_SDK_AUDIT.md). Hardware acceptance remains separate.
