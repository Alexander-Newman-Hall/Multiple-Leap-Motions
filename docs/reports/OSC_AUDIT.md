# OSC and processor baseline audit

Audited against `handbridge-v2` base `5f547181f6d512ab6a8d2146022227e26cec48d2` on 2026-11-15. Line references below describe that base unless stated otherwise. Source observations establish implemented behavior; physical Windows/TouchDesigner receipt remains **UNTESTED ON HARDWARE**.

## Actual path and reuse decision

`AppCore.cpp:60-64` routes sorted frames into `DataProcessor`. Its OSC callback (`AppCore.cpp:108-120`) calls `OscSender::sendOscMessage` directly. `OscController` is constructed (`AppCore.cpp:154-158`) but its latest-scalar worker is not the sender used by that callback. Retain the actual `DataProcessor`, hand value types, OSC message DTO, and oscpack serializer. Repair sender observability and contention; do not replace serialization with a newly invented protocol.

`OscController.h:61-90` stores only one latest scalar message, so using that abandoned path to transmit a complete frame would discard other joint channels. Its declarations also have many missing out-of-line definitions. Retire it only after the headless replacement has parity and rollback coverage; do not wire it into the new engine.

## Exact default and address contract

- `ConfigManager.cpp:83-84` uses `127.0.0.1:9000` when a successfully parsed file lacks those fields. Baseline constructor/member settings were uninitialized when no file loaded; this increment adds explicit constructor-safe defaults using those same fallback values. `README.md:73` instead shows saved `osc_port:7000`; that is an example, not the compiled default. The repository contains no deployed user config to identify which port a particular installation actually uses. Preserve a loaded valid port.
- UI port storage starts at zero (`UIController.hpp:119`) and is initialized from the config (`AppCore.cpp:213`). The UI accepts 0..65535 (`MainAppWindow.cpp:633-634`) although sending port zero is invalid for the new validated transport/config path.
- Processor defaults (`03_DataProcessor.cpp:25-29`) and UI defaults (`UIController.cpp:15-20`): palm, wrist, all five fingertips, pinchStrength and grabStrength ON; palm orientation, velocity, normal, visibleTime and finger extension OFF. Config initialization invokes the complete 14-flag callback (`UIController.cpp:89-121`).
- `03_DataProcessor.cpp:35-43`: addresses are `/leap/{alias}/{handType}/{dataType}[/{subType}]`; one float32 argument. The poller supplies lowercase `left`/`right`. No bundles or timestamp messages. `04_OscSender.cpp:80-83` uses `osc::BeginMessage` then one float. `sendMessages:92-101` flattens even a multi-value DTO into separate packets to the same address.
- Positions are the raw `HandData` vectors (`03_DataProcessor.cpp:129-159`), produced in SDK millimetres; no scale, axis swap, smoothing, gain, clipping or normalization is applied. The normalization constants and motion state are unused. Velocity is raw SDK mm/s, normal dimensionless, orientation a quaternion WXYZ ordering, visibleTime converts microseconds to seconds (`181-183`).

A complete valid default hand generates **23 datagrams** in this order:

| Relative address after `/leap/dev1/left/` | Count | Default |
| --- | ---: | --- |
| `palm/{tx,ty,tz}` | 3 | ON |
| `wrist/{tx,ty,tz}` | 3 | ON; valid arm required |
| `pinchStrength`, `grabStrength` | 2 | ON |
| `finger/{thumb,index,middle,ring,pinky}/{tx,ty,tz}` | 15 | ON; valid finger and distal bone required |
| `finger/{thumb,index,middle,ring,pinky}/isExtended` | 5 | OFF; emitted inside finger loop independently of position toggle |
| `palm/orientation/{qw,qx,qy,qz}` | 4 | OFF |
| `palm/velocity/{vx,vy,vz}` | 3 | OFF |
| `palm/normal/{nx,ny,nz}` | 3 | OFF |
| `visibleTime` | 1 | OFF |

All channels enabled generate **39 datagrams**. Fingertip index order is fixed by array slot rather than `fingerId`; a short finger vector omits absent slots. Invalid wrist/finger geometry is omitted, not zeroed. HandData's top-level `valid` flag is not inspected by this legacy processor. Do not assert stronger validity semantics for the original path.

## Legacy disappearance quirks, deliberately preserved

`03_DataProcessor.cpp:115-124` tracks seen handedness by alias. A subsequent frame without a formerly present hand emits zeros once, then forgets it. Empty frames from the outset emit nothing. The original device/service loss path does not inject this empty frame, so original hardware disconnect can leave a receiver's last pose indefinitely. The new supervisor can submit an empty frame on a loss/stale transition without altering the serializer.

The default disappearance produces **28 messages** (`47-78`): palm3, wrist3, then each finger's coordinates3 + `exists=0`, then pinch/grab. `exists=1` is never sent on live frames. The disappearance order differs from live order (pinch/grab are last). With all channels enabled loss sends 34 messages: the same 28 plus 5 finger extension zeros and visibleTime zero. Orientation, velocity and normal **are not zeroed**. Extension zeros depend on fingertip position toggles even though live extension messages do not. This is a known compatibility limitation, not an explicit validity protocol. Golden tests freeze the quirk instead of silently fixing deployed receiver semantics. Optional v2 validity remains deferred.

## Hand selection

`DataProcessor.cpp:107-112` asks `DeviceAliasManager::getAssignedHand`; that method currently always returns Both. The actual assignment filter is `LeapSorter`, and saved assignments belong to `ConfigManager`. Preserve the sorter and make runtime configuration restore explicit; don't claim the alias-manager TODO is a working persisted hand selector.

## Confirmed risks versus field failure hypotheses

Confirmed source defects/limitations:

- OSC sender and AppCore print/log per scalar (`04_OscSender.cpp:93-97`; `AppCore.cpp:111-117`), with `std::endl` flushing. This consumes the processing budget. It is a plausible backlog contributor but **not proven to cause the reported field stall**.
- No checked send result/counters reach the legacy runtime; failures only print, and no automatic socket recreation follows a failed initialization/send (`04_OscSender.cpp:45-48,86-88`). A temporary failure can persist unnoticed.
- UI target update (`AppCore.cpp:200-207`) can replace sender socket concurrently with processing sends unless ownership is made explicit. Sender previously had no mutex. This is a source-level data race, not a reproduced field failure.
- Legacy queue processing, service loss and stale stream recovery need independent lifecycle evidence; successful OSC sends never imply TouchDesigner receipt.
- `sendBundle` and raw `send` are stubs at the base (`04_OscSender.cpp:28-31,63-70`); stale `test_OscSender.cpp:134-146` incorrectly expects bundle output. Its receiver also has an unsynchronized shared buffer and startup/bind race. Preserve historical test sources but use new reliable tests.
- `test_DataProcessor.cpp` mixes removed APIs/constructors and `test_DataProcessor_HandPresence.cpp` calls nonexistent `getVizState`. Neither is evidence of working baseline parity.

## This increment

- Retain the exact processor math/address/type/default/loss behavior; allow omitted callbacks so a headless caller need not manufacture a UI object.
- Make AppLogger compile on Windows and SDK-free Linux; Windows continues using OutputDebugStringA.
- Retain oscpack message serialization and public legacy void entry points; replace only its UDP socket with a small native checked nonblocking RAII wrapper. The pinned oscpack `ip/posix/UdpSocket.cpp:206` and `ip/win32/UdpSocket.cpp:199` discard the return from `send()`, so their void call cannot establish OS acceptance. The replacement checks native results, preserves errors and adds metrics/timestamps and a mutex around socket/packet buffer/target/status. Invalid host/port, unavailable socket and payload limits fail visibly. No receiver acknowledgement is inferred.
- Remove sender success/per-scalar logging (root also removed AppCore scalar callback logging); errors count each failure but log once until a successful open/send, capped at one log per second even when the OS alternates send success and asynchronous ICMP errors. `lastError` retains the last observed error even after recovery for diagnostics. Socket initialization failures are visible through readiness/lastError; they do not count as packet send errors until a send is attempted.
- Implement the existing raw byte send interface, maximum UDP payload 65507; retain `sendBundle` as explicitly unsupported. Automatic retry belongs to the supervisor; `setHost` explicitly recreates the socket.
- `tests/fixtures/osc/` freezes full default, optional-channel and disappearance snapshots. `test_LegacyOscContract.cpp` uses a bound ephemeral loopback socket and its own OSC padded-string/network-float parser, validating actual packets without oscpack decoding. Additional tests cover filtering, invalid joints, independent devices/loss/reacquisition, target changes, multiple-value flattening and checked failures/reset. A valid broadcast destination without SO_BROADCAST forces a native permission error, asserting zero success packets and exactly one failed send attempt. No broadcast packet is sent.

## Validation

**CLOUD Linux:** `PATH="$PWD/work/tooling/bin:$PATH" cmake --build --preset core-debug --target test_LegacyOscContract -j 4 && out/build/core-debug/test_LegacyOscContract` — completed successfully; **8/8 tests passed**, 4 ms reported by GTest. Actual packet tests decode 23 default, 39 all-enabled and 28 disappearance native UDP datagrams, plus independent second-device output/reacquisition. Native broadcast error reported `connect: Permission denied`, with no packets sent and one counted failed attempt. This verifies a real Linux OS failure, not Windows error behavior.

Physical Windows Hyperion, two USB devices, TouchDesigner receipt, Windows native kernel-error injection and hardware soak remain **NOT RUN**. The SDK-free Windows socket/packet/readiness tests are recorded in final CURRENT_STATE.

### Observed Windows portability result

Windows CI at source060ee47 compiled the engine and passed all other tests,
but its UDP provider accepted the broadcast send used to provoke POSIX denial.
The kernel-denial case is now skipped explicitly on Windows; do not infer Windows
kernel-error coverage from the Linux denial result. Windows closed/readiness
failure accounting and golden packet tests remain active. No transport behavior
was changed to manufacture a failure; isolated native Windows syscall fault
injection is deferred. See final state/handoff for rerun counts.
