# Test engineering — unit, mock, integration and physical validation

## Test levels

**SDK-free unit tests:** alias mapping, configuration/migrations, channel selectors,
time-based smoothing and reset, transforms, OSC golden packets, payload limits,
validity/loss behavior and invalid input.

**Mock process integration:** two independent simulated devices; actual localhost
UDP receiver; control IPC and apply acknowledgements; frame stalls/reconnects;
profile persistence; GUI close/kill/restart while the engine continues sending.
These execute production processing/serialization, with synthetic tracking input.

**Genuine-SDK-type adapter tests:** production LeapC adapter compiled against the
official Hyperion 6.2 header, with injected C function pointers using its exact
declarations. No substitute SDK header, SDK binary or device is used. These test
ownership, lifecycle, recovery and version policy, not the actual service.

**Windows client/deployment checks:** load the actual external DLL, verify client
version/exports and run bounded no-service startup/cleanup; extract the portable
ZIP and launch with developer search paths removed. These require Windows but no
cameras, and are distinct from a successful SDK-header compile.

**Physical Windows acceptance:** discovery, two real streams, USB hotplug, service
restart, no-hands, soak/latency, native display/GPU/DPI behavior and actual
TouchDesigner receipt. See [HARDWARE_TEST_PLAN](HARDWARE_TEST_PLAN.md).

## Current verification checkpoint — 2026-11-16

| Scope | Actual result |
|---|---|
| Linux GCC 14.2 / Qt 6.8.3 / genuine Hyperion 6.2 header | Configure/build passed. |
| Full SDK-enabled native CTest suite | **82 passed, zero failures/skips; 20.48 s.** |
| Adapter tests within that suite | **21 passed**, genuine SDK types and injected functions; no SDK binary calls. |
| SDK-off native checkpoint before additive service-version fields | **61 passed, zero skips; 20.33 s.** |
| Official 6.2 installer extraction/hash verification | Passed; installer was not executed and no service was installed. |
| Current Windows Release / genuine 6.2 header | **81 passed + one expected POSIX-only skip**, zero failures; 32.03 s. Three strict lifecycle repetitions also passed. |
| Windows real client DLL 6.2.0.0 load/version/API checks | **PASS**, including bounded actual SDK startup/cleanup without a service or cameras. |
| Extracted Windows ZIP without developer PATH | **16 checks passed**; clean source manifest, actual Windows rendering, app-local Qt/CRT and continuing UDP. |
| Project adapter/process ASan+UBSan | **34/34 passed**, leak checking, zero diagnostics; excludes Qt/SDK binary instrumentation. |
| Physical cameras / Hyperion recovery / USB / TouchDesigner | **UNTESTED ON HARDWARE.** |

SDK-enabled Linux command used:

```sh
cmake --build out/build/leapc-control --parallel 2
ctest --test-dir out/build/leapc-control --output-on-failure
```

That build enables `HANDBRIDGE_BUILD_CONTROL` and `HANDBRIDGE_WITH_LEAPC`, points
`LEAP_SDK_ROOT` at the exact SDK extracted from the official 6.2 Windows installer,
and uses native offscreen/software Qt. The XML report is retained locally under
ignored `work/hyperion6-native-results.xml`. Configure instructions, exact SDK
provenance and current branch/commit are in [LEAPC_SETUP](LEAPC_SETUP.md),
[LEAPC_SDK_AUDIT](reports/LEAPC_SDK_AUDIT.md), [CURRENT_STATE](CURRENT_STATE.md) and
[SESSION_HANDOFF](SESSION_HANDOFF.md). Earlier plugin-derived 5.30 checks do not
establish the current 6.2 build or release baseline.

## Adapter acceptance covered without cameras

The 21 adapter tests cover multi-device-aware connection setup, service handshake,
two-pass serial queries, independent subscriptions, borrowed frame/hand data copied
before the next poll, native hand IDs, empty-hand frames, duplicate SDK IDs/serials,
device loss and rediscovery. Targeted restart must actually unsubscribe, close,
reopen and resubscribe A while B remains active.

Fault cases include create/open/info/subscription failure, bounded polling,
handshake timeout, fatal poll error, service loss, capped retry and idempotent
cleanup. Tests feed the production runtime to check A loss clears A output while B
continues, and stale A restarts then accepts reset frame IDs. Version cases reject
5.x, 6.1 and failed server-version queries before any healthy service state or
device enumeration; 6.2 and later 6.x are accepted. Actual hand-ID changes also
reset processing history rather than blending a newly acquired hand with the old
pose. None of this proves USB/service recovery on a physical PC.

## Controller, processing and protocol acceptance

Settings tests exercise actual scalar OSC output for channel/mute/hand/alias/rate,
dead-zone, time smoothing, transforms/units, raw bypass and opt-in hysteresis.
Legacy golden fixtures remain unchanged. Controller-model tests withhold or
delay acknowledgements and inject stale responses, conflicts and reconnects to
verify that editing is distinct from applied engine state. Missing-runtime
hardware preflight executes the sibling engine probe twice, checks actionable
error/busy-state recovery, and starts no owned engine.

Native QML tests type a new destination port and click Apply directly, then observe
real packets on the new receiver and GUI output-switch stop/resume. Applied preview
tests keep unacknowledged units out of live projection and retain raw millimeters
under bypass. Integration closes/kills/reopens actual GUI processes while engine
PID and changed OSC values continue. This is native Qt rendering, not a browser
mockup. Automated interaction covers selected critical paths; it does not certify
every control, keyboard/high-DPI layout, GPU or visual detail on Windows hardware.

IPC tests cover more than one drain batch of commands, malformed/version/oversized
frames, an unread-client flood, delayed endpoint availability, duplicate engine
ownership even with another state directory, revision CAS and diagnostic redaction.
Profile tests inspect actual profile/current/backup files and config/output round
trips. A production ConfigurationStore regression delivers an old save after a
newer save and checks current/backup bytes remain unchanged. Discovered devices
after restore advance revision and are correctly shown as unsaved.

## Deterministic fixtures and OSC checks

- Use fixed serials, aliases, frame counters, timestamp domains, handedness and
  known joint vectors; include empty/lost/reacquired and duplicate/old frames.
- Inject monotonic test time for liveness/retry tests rather than sleeping.
- Decode OSC independently and assert exact addresses, case, float types, units,
  channel toggles, default port and loss behavior before changing legacy output.
- Bind an ephemeral localhost receiver before process startup; verify per-camera
  isolation and changed settings in received packets, not just a UI property.
- Do not claim optional bundles, file replay, OSC v2 or full hardware acceptance
  from tests of the currently implemented scalar legacy/synthetic path.

## Reporting and retained historical evidence

Record date, source commit, OS/toolchain/SDK versions, actual commands, pass/fail/
skip counts and logs in CURRENT_STATE and SESSION_HANDOFF. A successful UDP send
does not prove receipt by TouchDesigner. Missing hardware is a blocked acceptance
gate, never a passing test.

The original Phase 0/1 checkpoint had 40 CTest entries; source/compile exclusions
and sanitizer commands are preserved in [BUILD_AUDIT](reports/BUILD_AUDIT.md).
The controller milestone at `ebb3bd9` passed 58 Linux entries and native Windows/
Ubuntu push and PR CI. Windows also passed three until-fail lifecycle repetitions
per event. Its separate ASan/UBSan checkpoint passed 56 entries with leak checking,
excluding the two GUI bundles and third-party Qt instrumentation. These historical
runs do not claim sanitizer or Windows coverage of the new SDK increment. Detailed
controller evidence is retained in [GUI_BUILD](reports/GUI_BUILD.md).

One actual broadcast-permission kernel regression is POSIX-only: Windows accepts
that send, so that specific case explicitly skips there. Other socket failures
and decoded UDP/runtime/config checks still run. Always report the actual Windows
pass/skip totals; this platform-specific skip is unrelated to hardware validation.

For Release reproduction and the verified no-development-PATH package gate, see
[WINDOWS_RELEASE](WINDOWS_RELEASE.md). A successful package smoke still requires
subsequent physical acceptance and a long-run soak on the target Windows PC.

## M2 worker compatibility regressions (SDK-free)

`test_WorkerConcurrency.cpp` exercises the production SnapshotMailbox with concurrent
readers, coherent immutable records and retained values after replacement and
mailbox destruction. `test_WorkerLifecycle.py` launches the existing QtCore/Network
host: a real-provider startup error must exit exactly 1 without abort, and explicit
simulation must exit naturally with final snapshot and decoded loopback UDP.
A timeout/kill is a failure, never a successful stop. No cameras, GUI target or SDK
are used by these checks. The full existing Windows/Linux UI/IPC suites still
verify output continuity across controller close/kill/reopen.

```sh
ctest --preset control-debug -R 'WorkerConcurrency|WorkerLifecycle' --output-on-failure
```

The current Mac validation workflow targets **native Apple Silicon arm64 only**.
It builds the Qt-free engine and the existing Qt engine/GUI, checks application
slices and executes full mock/control/UI/OSC/lifecycle suites. It separately loads
the genuine pinned arm64 SDK; adapter/header injected tests are enabled only when
that SDK step passes. A native Cocoa GUI smoke checks captures and continuing
simulated UDP after GUI close/kill/reopen. None establishes physical camera health.

The existing broadcast error case remains enabled. A new Darwin-only death-test
child explicitly defaults and unblocks SIGPIPE and checks failed sends/counters,
zero accepted packets and unchanged global disposition. Production sets only
SO_NOSIGPIPE per socket. No SIG_IGN or skip hides the original failure. Every
independent workflow outcome participates in its final failure gate.

Arm64 acceptance needs actual arm64 execution. Historical Intel results and Linux
regressions are not substitutes; if hosted native capacity fails, record BLOCKED.
No Intel, Rosetta or universal-release requirement remains. See MACOS_ARM64 report
for actual outcomes, commands and open protected-IPC/physical gates. No ASan/TSan
coverage is claimed by these ordinary builds.
