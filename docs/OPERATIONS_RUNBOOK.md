# Operations runbook — headless deployment and troubleshooting

**Current implemented operations:** `handbridge-engine --simulate [--headless]
[--config legacy.json] [--host 127.0.0.1] [--port 9000] [--duration-ms N]
[--scenario normal|stall]`, plus `--help` and `--version`. The process requires
`--simulate`, runs until Ctrl-C when duration is absent/zero, reads without
saving config, and prints transitions to stderr/final JSON to stdout. Exit 1 is
startup/config/argument failure; exit 2 reports observed send errors; exit 0 does
not prove a receiver exists. No status IPC or GUI exists yet. Use the build paths
in root README. Everything else below describes intended operations and must not
be treated as implemented.

## Planned launch models

- `handbridge-engine.exe --headless --config <file>` — run without GUI (proposed).
- `handbridge-control.exe` — attach/detach native control UI to the running engine.
- `handbridge-engine.exe --simulate --config <file>` — deterministic fake cameras for CI/development (proposed).
- Add `--print-status`, `--validate-config` and/or `--diagnostics` only if robustly implemented.

Do not label these commands functional until verified. Explicitly record supported flags in root README as they are delivered.

## Startup

1. Confirm Windows user permission, Ultraleap Tracking Service installed/running and cameras connected.
2. Start engine with saved profile; inspect status and current destination/port.
3. Launch TouchDesigner network with OSC In DAT/CHOP listener on the matching port.
4. Verify receiving data with hand present and disappearance behavior with hand absent.
5. GUI is optional; close it after setup without stopping engine.

## Failure response

- **UI broken but OSC alive:** Restart only UI; inspect IPC error and process logs; don't restart cameras.
- **One camera stale:** Check its last-frame age and SDK device state; keep other camera operating; inspect USB and subscription history; use targeted reconnect.
- **Both cameras stale:** Check service heartbeat and poll errors; verify Ultraleap service; capture logs before controlled recovery.
- **Frames processed but OSC not sent:** Check channel enable settings, output format/port, serializer/sender counters, socket errors.
- **OSC sender reports sends but TD sees none:** Verify TD listener address and port, OSC In DAT and Windows firewall/network assumptions; remember UDP gives no delivery acknowledgment.
- **Config disappears:** Inspect resolved config path, save errors, schema migration log, backup and alias persistence; do not overwrite corrupt originals before backup.

## Installations and Windows startup

Prefer an ordinary Windows user-session background executable, with optional startup-at-login after reliable operation is confirmed. Do not default to a privileged Windows Service because USB SDK/session access and GUI interactions may differ. Record expected behavior for machine sleep, user logout, OS updates and USB selective suspend in installation documentation.

## Diagnostics support bundle

Include redacted config schema version/selected port, application and SDK versions, runtime state transitions, last 5 minutes of bounded logs, per-camera frame ages, queue metrics and OSC output attempt/error counts. Do not include raw serial numbers or identifying machine paths when sharing publicly unless permission is explicit.
