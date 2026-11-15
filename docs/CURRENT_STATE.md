# Current state — 2026-11-15

Branch: `codex/handbridge-phase0-reliability`, based on `handbridge-v2`
`5f547181f6d512ab6a8d2146022227e26cec48d2`. Original remote `master` rechecked
at `054e578525a5d5202ea8f6eb417ce55ded7d61b8`; both input branches preserved.
Commits so far: audit `350249f`, config `36c8f9b`, OSC `c7e006e`.
Runtime/build commit is the next checkpoint, subject
`feat: add tested headless simulation and isolated recovery foundation`.
Final handoff will record its exact SHA and observed CI run.

## Delivered scope and gates

- **Phase 0 DONE:** complete code/build/test inventory with baseline filename/line
  references, actual legacy configure failure, exact source-backed OSC/config
  contracts, preserved README and CMake, dependency/SDK/licensing inventory.
- **Phase 1 first reliability milestone DELIVERED:** native C++20 headless
  simulation host, two independent mock sources, owned event/time envelope,
  per-device latest mailbox/counters, stale/loss invalidation, capped targeted
  retry and service rediscovery, checked OS UDP sends and golden compatibility.
  Default remains loopback:9000, raw mm, lowercase hands and scalar float packets.
- **Phase 1 broader gate IN PROGRESS:** Windows CI execution pending publication;
  replay-file CLI, FPS/latency/stage metrics, automatic output socket recreation,
  threaded poll worker and full versioned/atomic configuration are deferred.
- **Phase 2 BLOCKED ON INSTALLED SDK/HARDWARE:** no actual ITrackingSource LeapC
  adapter. Original Windows GUI remains reference with known liveness defects.
- **Phase 3–5 NOT STARTED:** Qt/IPC/controller lifecycle tests, advanced processing,
  profiles/v2 output/bundles and release packaging.

## Verification matrix

CLOUD environment: Debian 13 x64, GCC14.2, Python3.12.14, CMake3.31.6,
Ninja wheel1.11.1.4. Same final source tree tested in each configuration:

| Environment / command | Actual outcome |
|---|---|
| Original clean configure | FAIL: required OpenGL missing; full legacy compile never reached |
| CLOUD `cmake --build --preset core-debug --parallel 2`; `ctest --preset core-debug` | 40/40 PASS, zero skips; 3.76 s |
| CLOUD `cmake --build --preset core-release --parallel 2`; `ctest --preset core-release` | 40/40 PASS, zero skips; 3.73 s |
| CLOUD ASan+UBSan Debug configure/build/CTest | 40/40 PASS, zero skips; 4.11 s, no sanitizer diagnostics |
| CLOUD independently decoded actual UDP | PASS: goldens, A stall/clear/recovery while B continues, config aliases/channels |
| CLOUD actual kernel send denial | PASS: no false sent packet; error counted |
| WINDOWS CI | CONFIGURED, NOT YET OBSERVED at this checkpoint |
| Windows original SDK application | NOT BUILT; explicit SDK path target available, unverified |
| Physical Hyperion/two cameras/USB/service/TouchDesigner/soak | UNTESTED ON HARDWARE |
| Qt UI attach/close/crash/reopen | NOT RUN, no controller yet |

Exact local-tool commands, pins, exclusions and sanitizer scope:
[BUILD_AUDIT](reports/BUILD_AUDIT.md). JUnit local artifacts:
`work/core-{debug,release,sanitized}-results.xml` (ignored).
CI publishes test reports only. Python is a test receiver, not the engine.

## Known limitations

Real silent OSC stoppage remains unproven; confirmed code defects are not a field
root-cause claim. Original adapter recovery is unchanged. Optional orientation/
velocity/normal retain legacy loss quirks; no v2 validity is emitted. Legacy save
is checked but non-atomic; new host reads explicit legacy config without saving,
rejects proposed v2 and does not load Windows AppData implicitly. No lost aliases
can be reconstructed from old null maps. Single owner source poll must be bounded;
a blocked future SDK call still needs enforced poll timeout/thread supervision.
No automatic UDP socket recreation/backoff exists yet.

## Single next recommended task

**P2.1: Add the smallest real LeapC ITrackingSource adapter and Windows SDK build
seam against exact installed headers.** Record SDK/service versions; use one owner
and RAII for connection/device/subscriptions, owned frame copies and bounded poll.
Keep the mock/golden suite green. Compile on a configured Windows x64 SDK PC,
then record two-device discovery/frame/OSC and loss evidence; mark physical work
BLOCKED if the machine/devices are unavailable. Do not start GUI/processing work.

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

CI artifact correction: the workflow now writes JUnit XML to an absolute workspace
path so the existing artifact step includes both XML and LastTest.log. This is
build/report plumbing only; runtime source remains060ee47, Windows test correction
a18132b. Final rerun is pending.
