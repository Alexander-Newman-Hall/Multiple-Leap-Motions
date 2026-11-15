# Running the native controller and engine

Build instructions: root README and reports/GUI_BUILD.md. With the Qt build,
run handbridge-control --demo to start an independent simulated engine, or run
handbridge-engine --simulate explicitly then connect the controller. Both use
the same default per-user local endpoint. No GUI is needed for OSC.

Closing/crashing the window intentionally leaves the detached engine alive.
Reopen the controller to reconnect. To stop a foreground host, use Ctrl-C; a
background host can be stopped explicitly in the OS process manager. No elevated
service, autostart installer or hidden OS service restart is installed.

Edit settings then Apply; only engine acknowledgement is Applied. Save a named
profile to write the applied snapshot/current startup state atomically with a
backup. Dirty local editors cannot be saved/loaded silently. Rejected settings
leave the last valid engine state. A profile load can apply then encounter a
persistence error; read its explicit applied-but-unsaved response.

State/profile directory is private per-user HandBridge data, overridable by
--state-dir. Explicit --config accepts legacy or implemented schema2 in the Qt
host. Old flat config is imported without overwriting it. Default plain Qt-free
host still accepts legacy flat config only. New source is simulation-only.

Device restart and output-socket reset are explicit UI commands. Diagnostics
exports bounded serial-redacted state/config/logs into the engine diagnostics
folder and reports its actual path. Send counts describe OS acceptance, never
TouchDesigner receipt. See CONTROL_PROTOCOL.md and CONFIG_SCHEMA.md.

## When output stops

Inspect independent service/device/subscription/freshness/hand/processing/send
metrics. No hands is healthy if frames continue. A stale camera is invalidated
and retried independently; the other camera continues. Verify destination/channel
selection and the actual TD listener before attributing a socket success to
receipt. Capture diagnostics before a targeted restart.

Real camera/Hyperion/USB recovery and TouchDesigner receipt remain UNTESTED ON
HARDWARE until the local Windows adapter/test task. Physical workflow and test
requirements are in WINDOWS_LOCAL_HANDOFF.md and HARDWARE_TEST_PLAN.md.
