# Session handoff — configuration checkpoint

Date: 2026-11-15. Branch `codex/handbridge-phase0-reliability`.
Base `5f547181f6d512ab6a8d2146022227e26cec48d2`; audit `350249f`.
This checkpoint commit subject: `fix: preserve legacy aliases and initialize configuration safely`.
Original `master` and `handbridge-v2` preserved.

Changes: `src/core/ConfigManager.{cpp,h}`, `DeviceAliasManager.{cpp,hpp}`,
`tests/test_ConfigManager.cpp`, `test_DeviceAliasManager.cpp`,
`configs/legacy-default.example.json`, `docs/CONFIG_SCHEMA.md`,
`docs/reports/CONFIG_AUDIT.md`, current state and this handoff.

CLOUD verification: direct GCC14.2 C++20 build using pinned JSON/GoogleTest
sources extracted by CMake. Run `work/config-tests --gtest_output=xml:work/config-tests.xml`:
17/17 PASS (10 config, 7 aliases), zero skipped. Full command and later reproducible
CMake replacement are recorded in the final build report/checkpoint. Tests cover
exact alias shape, reversed discovery, all flags/ports/assignments, malformed and
unsupported versioned config, late-field rollback, index bounds and duplicates.

No compatibility port/address changes. Null aliases from the broken old saver
cannot be recovered without external evidence. Saved config keeps legacy fields;
atomic replacement/backups and v2 profiles remain deferred. New source validation
does not establish Windows file-path or physical tracking behavior.

Single next task within this session: freeze actual scalar OSC bytes/loss quirks
and integrate the isolated mock runtime/build. Physical Windows SDK, cameras and
TouchDesigner remain UNTESTED ON HARDWARE; no field stall fix is claimed.
