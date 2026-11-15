# Local native control protocol v1

Implemented by the optional QtCore/Network engine and separate Qt Quick client.
No HTTP/WebSocket server, remote endpoint or account is involved.

## Transport and ownership

QLocalServer/QLocalSocket: same-user UserAccessOption. The default endpoint is
`handbridge-<first16 SHA256(homePath)>-v1`; explicit endpoint tokens use1..80 ASCII
letters/digits/dot/underscore/hyphen. An endpoint lock in private per-user appdata
is independent of `--state-dir`; a second engine cannot replace an active endpoint.
Do not infer Windows peer/USB behavior from Linux socket tests.

Each message is four-byte unsigned big-endian payload length followed by UTF-8
JSON object. Maximum payload64KiB; zero/oversize/invalid JSON disconnects the
client. Read/write buffers are bounded256KiB; at most8 clients and32 queued engine
commands. Parse at most16 frames per event-loop turn, then schedule a drain for
complete buffered frames. A slow/unread client is disconnected; it cannot block
the source/OSC worker. Telemetry is sampled at10Hz with bounded128-line logs.

The source/runtime/sender have one independent C++ worker. IPC parsing and disk
persistence run on the QtCore thread. The GUI sends commands and renders copies;
it never owns the engine. Starting a simulator uses a detached process; closing,
killing or restarting the controller leaves the engine running.

## Requests and responses

```json
{"version":1,"id":12,"command":"apply_config","expected_revision":3,"config":{"schema_version":2}}
```

Nonnegative integer ids and revision numbers are bounded by exact JSON integer
precision (2^53-1). Unsupported versions/commands reject. Apply/load require an
expected revision; GUI saves include it too.

| Command | Fields | Behavior |
|---|---|---|
| `get_state` |— | Full canonical config/state/revision |
| `apply_config` |`expected_revision`, `config` | Strict validation, new target socket ready before commit, acknowledged at worker boundary |
| `list_profiles` |— | Saved names |
| `save_profile` |`name`, optional `expected_revision` | Save applied snapshot and startup restore atomically with backups |
| `load_profile` |`name`, `expected_revision` | Validate/apply saved snapshot; persist startup restore |
| `restart_device` |`serial` | Invalidate/restart one source, other devices continue |
| `reset_output` |— | Recreate output socket, tracking continues |
| `export_diagnostics` |— | Write bounded serial-redacted JSON in private diagnostics directory; return path |

Responses include `version`, `id`, `ok`, optional `error`, `revision`,
`saved_revision`, `config`, `state`, `logs`, `profiles`; export also returns `path`.
A rejected apply retains the previous snapshot. A load persistence failure can
follow an applied snapshot: the error explicitly says applied-but-unsaved and
returns actual state. The client must not infer state solely from a success flag.
Saved revisions never regress because of delayed callbacks.

Unsolicited telemetry has `event:"state"`, no request id, and the same canonical
state payload. State includes simulated/source/hardware_verified/service fields,
a devices array (serial/alias/presence/subscription/freshness/hands/age/FPS/counts/
retries and raw/processed observed point previews), cumulative native packet/error/
byte counters, target/readiness, measured packet rate and socket generation.
OS send success never indicates receiver acknowledgement.

The client retains dirty edits during telemetry, rejects stale CAS, ignores older
responses in a live connection, and refreshes after reconnect. A new connection
may have lower revisions after an engine restart. Saved/Applied/Editing/Rejected
are distinct; field appearance never substitutes for an engine acknowledgement.

## Host and controller CLI

Qt engine: `handbridge-engine --simulate [--headless] [--config file]
[--host IPv4] [--port N] [--endpoint token] [--state-dir private-dir]
[--scenario normal|stall] [--duration-ms N]`. Also `--help`/`--version`.
The source is simulation-only; no real LeapC mode is implied.

Controller: `handbridge-control [--demo] [--endpoint token]`. `--demo` starts a
simulator only if no engine is connected; closing the window does not stop it.
Automation supports `--exit-after-ms`, `--page overview|devices|processing|osc|profiles`
and `--screenshot path.png`. These render the actual native window, not a mockup.

Official references verified: [QLocalServer](https://doc.qt.io/qt-6/qlocalserver.html),
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html),
[Qt Quick](https://doc.qt.io/qt-6/qtquick-index.html). Exact Qt6.8.3 C++ development
files were used in the cloud build. Hardware acceptance remains separate.
