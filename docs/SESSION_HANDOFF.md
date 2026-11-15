# Session handoff — Phase 0 + first Phase 1 reliability slice

Date: 2026-11-15. Environment: Linux cloud; no installed LeapC SDK or devices.
Branch `codex/handbridge-phase0-reliability`.
Base `handbridge-v2`: `5f547181f6d512ab6a8d2146022227e26cec48d2`.
Preserved `master`: `054e578525a5d5202ea8f6eb417ce55ded7d61b8` (remote rechecked).
Commits: audit `350249f`, config `36c8f9b`, OSC `c7e006e`; runtime/build checkpoint
subject `feat: add tested headless simulation and isolated recovery foundation`.
A documentation-only follow-up will pin that SHA and actual CI outcome.

## Changed files and behavior

- Audit: `docs/LEGACY_AUDIT.md`, `docs/reports/{TRACKING,CONFIG,OSC,BUILD}_AUDIT.md`,
  original `docs/legacy/README.md`, original `cmake/legacy-original.cmake`.
- Config: `src/core/ConfigManager.{cpp,h}`, `DeviceAliasManager.{cpp,hpp}`,
  two corresponding regression tests and `configs/legacy-default.example.json`.
  Source-backed defaults and aliases repaired; malformed/versioned inputs reject
  transactionally; original valid port/keys preserved. Save not yet atomic.
- OSC: `src/pipeline/03_DataProcessor.cpp` optional callbacks,
  `04_OscSender.{cpp,hpp}` checked native nonblocking sockets/stats/locks,
  portable `AppLogger.hpp`, removed scalar logs from `AppCore.cpp`;
  three `tests/fixtures/osc/*.json`, `test_LegacyOscContract.cpp`.
- Runtime: `src/runtime/{TrackingSource,MockTrackingSource,EngineRuntime}.*`,
  `engine_main.cpp`, `tests/test_EngineRuntime.cpp`, `test_HeadlessUdp.py`.
  Source events carry serial/frame identity, source and monotonic arrival clocks;
  one pending frame/device, independent stale/retry, honest service/device/output
  health. New executable requires `--simulate`; CLI never auto-saves legacy config.
- Build: `CMakeLists.txt`, `CMakePresets.json`, `cmake/LegacyWindows.cmake`,
  `.github/workflows/core-mock.yml`, `.gitignore`; SDK include spellings fixed in
  pipeline `00_LeapConnection.cpp`/`01_LeapPoller.cpp`. No SDK API symbol changes.
- Docs: root README/spec/plan; architecture/ADRs/protocol/config/TD/testing/
  dependencies/reliability/operations/sources/workflow/changelog/current/handoff
  and hardware result status. Original tracked `build/` is untouched.

## Exact cloud commands and results

Tool bootstrap (local ignored venv only):

```sh
python3 -m venv work/tooling
work/tooling/bin/python -m pip install cmake==3.31.6 ninja==1.11.1.4
work/tooling/bin/cmake --preset core-debug -DCMAKE_MAKE_PROGRAM="$PWD/work/tooling/bin/ninja"
work/tooling/bin/cmake --build --preset core-debug --parallel 2
work/tooling/bin/ctest --preset core-debug --output-junit "$PWD/work/core-debug-results.xml"
```

**40/40 PASS**, zero skips, 3.76 s. Repeat with `core-release`: **40/40 PASS**,
3.73 s. ASan+UBSan command in BUILD_AUDIT: **40/40 PASS**, 4.11 s, no diagnostics
(project core/runtime/sender/tests instrumented, third-party libraries excluded).
Config-only checkpoint: 17/17; OSC-only checkpoint: 8/8. The final 40 cases include
13 mock/runtime and one real-process Python UDP test. No discarded obsolete tests
are counted as passing; their actual compile failures are in BUILD_AUDIT.

Baseline original clean configure: FAIL at missing OpenGL; SDK/GUI compile not
reached. Optional legacy target on Linux correctly rejects platform. Git diff
whitespace checks pass; source/fixture tree unchanged after the final tests.

## What is verified versus assumed

Verified CLOUD: actual preserved processor+serializer packets decoded independently;
raw mm/lowercase/float/per-scalar/default/loss quirks; config round trip and rejection;
two mocks independent across stale/disconnect/service/subscription failures;
100 reconnect cycles; queue overwrite/duplicate/stale rejection; output failure
visible/recoverable without stopping tracking; actual kernel send failure; GUI absent.

Not verified: Windows native socket/path/compiler behavior until CI result is
observed; installed LeapC ABI/API compilation; USB/Hyperion/two physical cameras;
TouchDesigner receive; UI attach/kill/reopen; hardware soak/CPU/latency. No receiver
acknowledgement is inferred from UDP send. Public official docs checked, installed
headers unavailable; see TRACKING_AUDIT for exact links/hashes/constraints.

## Known bugs / blockers

- Real silent OSC loss root cause remains unknown, not declared fixed.
- New runtime has no LeapC source; optional legacy GUI still has unaddressed
  service/subscription/freshness/UI liveness limitations from the audit.
- No recorded replay CLI, Qt/IPC, automatic socket recreation, full stage/FPS
  metrics, v2 protocol/bundles, atomic config backup/presets or packaging yet.
- Optional orientation/velocity/normal legacy values can remain held after loss;
  preserve compatibility and use runtime freshness, explicit validity later.
- Source poll must be bounded/nonblocking. Retry start is idempotent before service
  acknowledgement; a real adapter must enforce resource/thread ownership.

## Single next Codex task and acceptance

**Implement P2.1 real LeapC ITrackingSource adapter/build seam on the installed
Windows x64 SDK.** Start by recording exact SDK headers/service versions and
comparing official APIs; do not invent symbols. Add RAII lifecycle with explicit
subscriptions and owned frames routed by source device ID/serial. Enforce one
poll owner and bounded timeout; retain the current mock/OSC suite. Build the new
headless SDK target on the configured PC, then record two-device frames and OSC
observations using HARDWARE_TEST_PLAN; absence of devices must be marked BLOCKED.
Do not expand into GUI, fusion, processing or a full release in that task.

## First observed Windows CI and portability correction

Runtime/build source commit: `060ee47dd3897859504ac97c911a5a504754367a`.
Draft PR: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/pull/1
Run https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43766927335:
Ubuntu PASS40/40. Windows Server2022/MSVC19.44 build PASS; tests39 passed/1 failed
because the broadcast-permission test assumed POSIX kernel rejection. The Windows
provider actually accepted the send; this is a test assumption, not evidence of
incorrect checked-send results. The native-permission test is now explicitly
POSIX-only and skips on Windows; closed-socket/readiness failure and actual packet
receiver tests still run there. Next CI run will establish final Windows counts.
No physical hardware test was performed.

CI report-path follow-up: JUnit output uses an absolute workspace path; the first
run uploaded LastTest.log but placed relative XML in the binary tree. No runtime
source change; next run validates artifact collection too.
