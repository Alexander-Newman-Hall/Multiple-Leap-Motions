# Implemented legacy configuration and proposed v2 schema

## Implemented in Phase 0/1 (2026-11-15)

The new host accepts an explicit `--config` legacy JSON path; it reads without
saving. An explicitly missing/malformed config exits nonzero. With no path it
uses source-backed defaults in memory: `127.0.0.1:9000`, low latency false,
palm/wrist/all five fingertips/pinch/grab enabled, optional channels disabled.
The original Windows no-argument ConfigManager location remains
`%LOCALAPPDATA%/LeapApp/config.json`; no Linux default persistence is invented.

Working example: [legacy-default.example.json](../configs/legacy-default.example.json).
Persistent keys remain `osc_ip`, `osc_port`, `low_latency_mode`,
`hand_assignments`, `device_aliases`, `booleanSettings`. Saved port overrides
9000. Host requires numeric IPv4; loaded ports must be integers 1–65535.

Alias save now emits `"device_aliases":{"serial-A":"dev1"}`. Loading accepts
that flat map, root camelCase `deviceAliases`, and the observed nested
`device_aliases.deviceAliases` wrapper. Ambiguous containers, duplicate aliases,
invalid types and malformed indices reject without modifying prior settings.
A null alias map written by the old bug contains no recoverable identities.
See [exact audit and regressions](reports/CONFIG_AUDIT.md).

Hand assignment text accepts left/right/both/none/disabled/empty in either case.
The headless runtime normalizes case: left/right filter; both/none/empty pass
through; disabled suppresses that device's hand output while tracking health
continues. The original sorter's case-sensitive behavior remains a legacy defect;
this repaired selection belongs to the new host. Assignments and channel toggles
are exercised against real simulated UDP output.

Legacy load validates a complete candidate before committing. Unsupported
`schema_version` is rejected, including the proposed example below. There is no
v2 migration CLI, presets, atomic save/backup or configuration IPC yet. Existing
legacy setters remain source compatible. Legacy save is direct-write with checked
flush; the new host never invokes it automatically. Back up existing configs
before explicitly using the legacy application's save operation.

---

**Status:** Proposed v2 design. The example below is NOT guaranteed to work with the current legacy executable. Codex must implement, version, test and document this schema, and provide a migration path from the legacy `config.json`.

## Example schema (illustrative proposed configuration)

```json
{
  "schema_version": 2,
  "profile_name": "two-camera-default",
  "engine": {
    "headless": true,
    "telemetry_preview_hz": 20,
    "logging_level": "info"
  },
  "osc": {
    "enabled": true,
    "host": "127.0.0.1",
    "port": 9000,
    "protocol_mode": "legacy_v1",
    "bundles_enabled": false,
    "send_rate_hz": 60,
    "send_when_invalid": "legacy_compatibility"
  },
  "devices": {
    "serial-to-be-replaced": {
      "alias": "dev1",
      "enabled": true,
      "hand_filter": "both",
      "world_transform": {
        "enabled": false,
        "translation_m": [0.0, 0.0, 0.0],
        "rotation_quat_xyzw": [0.0, 0.0, 0.0, 1.0]
      }
    }
  },
  "data": {
    "raw_passthrough": true,
    "channels": {
      "palm_position": true,
      "wrist_position": true,
      "finger_tips": ["thumb", "index", "middle", "ring", "pinky"],
      "pinch_strength": true,
      "grab_strength": true,
      "palm_orientation": false,
      "palm_velocity": false,
      "palm_normal": false
    },
    "processing": {
      "smoothing": {"enabled": false, "algorithm": "exponential", "time_constant_ms": 25},
      "dead_zone": {"enabled": false, "distance_m": 0.001},
      "output_scale": [1.0, 1.0, 1.0],
      "output_offset_m": [0.0, 0.0, 0.0]
    }
  }
}
```

**This is an example, not a mandated field-by-field implementation.** Codex may simplify the schema, but must preserve meanings, provide a validator and migration tests, and note changes in ADRs.

## Invariants
- `schema_version` mandatory; unknown future versions are reported and not silently overwritten.
- Alias/serial assignments must persist and avoid collisions. This is an identified bug area in the old code (`deviceAliases` vs `device_aliases` nesting/key mismatch); add round-trip tests.
- `hand_filter` semantics must distinguish `both`, `left`, `right`, `disabled`. Do not overload `none` to mean 'show all'.
- Any UI/CLI/IPC change first validates type, range and compatibility; engine returns applied/rejected and updated config revision. Invalid change must not partially apply.
- Save atomically to temp + fsync/close as appropriate + rename/replace, with one backup or recovery option. Do not clobber legacy config without saving a backup.
- Use per-user Windows app data for writable settings; override with explicit `--config` for installations. Record resolved path in diagnostics without leaking credentials or personal file paths in shared logs.
- A preset is not a copy of every transient connection state. Distinguish persistent desired configuration from runtime health telemetry.
- Numeric units, smoothing algorithm and coordinate transform order must be documented. Defaults should preserve original raw-mm OSC output.
- UI's `Saved`, `Applied` and `Connected` are distinct states.

## Migration and testing
1. Capture representative original config files in sanitised fixtures, preserving alias mapping and hand assignments.
2. Detect any observed old structures (`deviceAliases`, `device_aliases`, nested containers) and migrate explicitly; report ambiguous input rather than dropping aliases.
3. Round-trip save/load checks all fields including OSC IP/port and per-channel toggles.
4. Test missing/invalid/corrupted file recovery, duplicate aliases, unsupported schema version and concurrent update revisions.
5. Provide `--validate-config <path>` and `--migrate-config <old> <new>` if implementation effort is proportionate; document exact CLI.
