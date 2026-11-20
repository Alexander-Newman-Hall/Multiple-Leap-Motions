# Engine settings and profiles

Both headless engine builds accept schema **2** and legacy flat configurations.
The engine sends legacy scalar OSC
by default. Enable `channels.sendFullSkeleton` for an additional OSC v2 skeleton
message per tracked hand. Use the [studio default](../configs/studio-default.example.json)
as a starting point.

## Implemented schema

Only `schema_version` is mandatory. Omitted fields get source-backed defaults;
unknown fields, wrong types, invalid ranges and unsupported versions reject the
whole candidate. Applying settings replaces a validated snapshot at a frame
boundary; it never partially applies a malformed candidate.

| Section | Fields / defaults / bounds |
|---|---|
| `schema_version` | Integer2 |
| `osc` | `enabled:true`, `host:"127.0.0.1"` numeric IPv4, integer `port:9000` in1..65535, `rate_hz:0` (source rate) or1..240 |
| `channels` | 15 booleans listed below |
| `processing` | Settings below; `bypass:true` by default |
| `devices` | Map of serial→`{alias,enabled:true,hand_filter:"both"}`; up to64, unique devN aliases |

Channel names: `sendPalm`, `sendWrist`, `sendThumb`, `sendIndex`, `sendMiddle`,
`sendRing`, `sendPinky`, `sendPinchStrength`, `sendGrabStrength` default true.
`sendPalmOrientation`, `sendPalmVelocity`, `sendPalmNormal`, `sendVisibleTime`,
`sendFingerIsExtended` and `sendFullSkeleton` default false. These switches
control OSC output. Channel and processing settings apply globally. Each device
has its own alias, enable switch and hand filter.

In **OSC Output**, turn on **Full hand skeleton** and click **Apply** to send
`/handbridge/v2/{alias}/{side}/skeleton`. Each message carries a hand pose in
the camera's coordinate system, with positions in millimetres. The skeleton
uses raw tracking data regardless of the processing settings and has its own
channel switch. The OSC enable switch, destination, send rate, device enable
and hand filter apply to both outputs. Save the applied settings to a profile
to restore the switch at startup. See the [OSC contract](OSC_PROTOCOL.md).

| Processing field | Default | Accepted values |
|---|---|---|
| `bypass` | true | Raw positions/values pass through unchanged in mm |
| `smoothing_ms` |0 |0..2000, time-based exponential position filter |
| `deadzone_mm` |0 |0..500, positional change deadzone |
| `scale` |[1,1,1] | Three finite numbers, each-100..100 |
| `rotation_deg` |[0,0,0] | Three finite numbers, each-360..360; X then Y then Z |
| `offset_mm` |[0,0,0] | Three finite numbers, each-10000..10000 |
| `output_units` |"mm" |"mm" or"m"; applied only when bypass is off |
| `gestures_enabled` |false | Independent opt-in pinch/grab hysteresis channels |
| `pinch_enter` / `pinch_exit` |0.8 /0.6 |0..1, exit<=enter |
| `grab_enter` / `grab_exit` |0.8 /0.6 |0..1, exit<=enter |

Processing order: select device/hand → raw-mm deadzone/time smoothing → scale →
Euler rotation → offset → units → legacy channel serialization. Velocity gets
scale/rotation/unit conversion without offset; normal/direction rotate, and
orientation quaternions compose the rotation. Native pinch/grab strength stays
unchanged. Filters reset on loss, restart, legacy routing changes and edits to
the processing settings. Switching the full skeleton stream on or off preserves
the legacy pose, smoothing history and gesture state.
The real adapter supplies native hand IDs, so a newly tracked hand starts with
a fresh filter history. The GUI disables processing controls during
raw bypass; enabling gestures is independent.

A device's `enabled:false` mutes its output while tracking health remains live.
`hand_filter` is `both`, `left`, `right`, or `disabled`; legacy empty/NONE imports
as both. Serial keys are literal, including dots. Aliases remain dev followed by
decimal digits, validated unique. Discovery appends stable aliases and advances
configuration revision, so newly discovered unsaved identities never show Saved.

## Migration and persistence

Both hosts import existing legacy `osc_ip`, `osc_port`, `booleanSettings`,
`hand_assignments` and alias wrappers via ConfigManager. Use `--config <path>`
to select a file. The `--host` and `--port` options override its destination
for this run. The Qt build also supports named profiles and controller IPC.
See the [baseline audit](reports/CONFIG_AUDIT.md).

Profiles and `current.json` are stored beneath the engine's private per-user
HandBridge data directory; `--state-dir` provides an explicit private override.
Save/load use QSaveFile atomic replacement, with the previous bytes retained as
`.bak`. Save a named profile to persist the applied settings for startup restore;
applied-but-unsaved changes do not overwrite startup state. On corrupt current
file, a readable valid backup is tried. A stale load callback cannot overwrite a
newer saved snapshot. No profile path traversal is allowed.

Load applies the profile, then persists startup restore on the IPC/file thread.
If persistence fails after apply, the response explicitly reports that settings
were applied but not saved, with authoritative config/revision. Applied and Saved
remain distinct. File I/O does not run on the tracking worker.

Choosing a hand filter in Devices immediately submits that camera’s selection. Other GUI edits stay local until Apply. The engine acknowledges both kinds of change. Requests use
`expected_revision` and conflicts reject without overwriting newer settings.
Pending profile loads prevent further editor changes. Decimal fields use a
consistent explicit parsing locale; validation in the engine remains authoritative.

Legacy direct save is still non-atomic; the new atomic profile mechanism belongs
to the Qt host. The original proposal is archived under docs/legacy.

## Runtime provider selection

Select real input at engine startup, or use explicit --simulate; select the
installed SDK client using --leapc-library or the documented environment variables.
These are process/dependency settings, not additional profile JSON keys. Do not
add an undocumented source/runtime path field to a validated profile. The real
client and connected service must each report 6.2.0 or newer 6.x. See LEAPC_SETUP.md.
