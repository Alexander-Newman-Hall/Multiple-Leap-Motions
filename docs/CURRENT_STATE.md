# Current state — 2026-11-15

Branch: `codex/handbridge-phase0-reliability`, based on `handbridge-v2`
`5f547181f6d512ab6a8d2146022227e26cec48d2`. Remote `master` rechecked at
`054e578525a5d5202ea8f6eb417ce55ded7d61b8`; both original branches preserved.

## Phase 0 checkpoint

- Complete source/build/test-path inventory is recorded in [LEGACY_AUDIT](LEGACY_AUDIT.md)
  and detailed [tracking report](reports/TRACKING_AUDIT.md).
- Original README preserved verbatim at [legacy/README.md](legacy/README.md).
- Official Ultraleap documentation checked; exact installed SDK is unavailable.
- Confirmed defects: UI-thread OSC consumption, stale pose on device loss, failed
  subscription reported connected, alias key mismatch, undefined first-run settings,
  silent queue drops and scalar logging. Field stall root cause remains unknown.
- Initial clean legacy configure and SDK-free replacement build results will be
  recorded in the implementation checkpoint. No hardware build is claimed.

Phase 0 inventory is complete; baseline tests/build and the bounded Phase 1 mock
vertical slice are in progress. Phase 2 hardware adapter, Phase 3 Qt/IPC and later
processing/profiles remain unimplemented.

## Verification

| Environment | Result |
|---|---|
| CLOUD source audit | COMPLETE, static code evidence |
| CLOUD compile/tests | IN PROGRESS |
| WINDOWS CI | NOT RUN at this checkpoint |
| PHYSICAL WINDOWS / two cameras / service / USB / TouchDesigner | UNTESTED ON HARDWARE |

Next task within this session: finish SDK-free baseline and isolated per-device
stale/retry tests, then record actual build/UDP evidence before declaring a gate.
