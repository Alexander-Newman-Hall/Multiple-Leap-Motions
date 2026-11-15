# TouchDesigner local OSC integration and compatibility

The primary use case is **Hand Bridge and TouchDesigner on the same Windows PC**. This document describes the *intended* version 2 workflow and currently identifiable legacy behavior. Verify exact names/ports against the installed build and `docs/OSC_PROTOCOL.md`.

## Connection steps

1. Launch Hand Bridge headlessly with a profile whose OSC destination is `127.0.0.1` and a chosen UDP port. The audited and retained fallback is **9000**; the original README shows a **7000** example. Valid saved settings override the fallback. The receiver and sender must match.
2. In TouchDesigner, create **OSC In CHOP** for numeric channels, or **OSC In DAT** for inspecting entire OSC message addresses and argument payloads.
3. Set the local listener/network port to the exact Hand Bridge sending port and enable receiving. Consult your installed TouchDesigner version's parameter names.
4. Verify actual incoming addresses in OSC In DAT first. Example current C++ paths use lowercase handedness, e.g. `/leap/dev1/left/palm/tx` (see legacy serializer).
5. Move a hand in view; confirm coordinates change. Remove hand; confirm the configured missing-hand semantics.
6. Test Camera B independently and simultaneously. Do not assume `dev1` always refers to a physical left-mounted camera until the serial→alias mapping is saved.
7. Save the TouchDesigner project and Hand Bridge profile together. Put both version strings or hashes in the installation notes.

## Suggested initial TouchDesigner test network

```text
[OSC In DAT: raw message log]    [OSC In CHOP: numeric channels]
               \                         /
                [Select / Rename CHOPs as appropriate]
                              |
                       [Null CHOP: out_hand]
                              |
                       [3D cursor test geometry]
```

This diagram is illustrative, not a .toe file. The fastest diagnostic is often OSC In DAT because it reveals exact address, argument count/type and order before channel naming assumptions are added.

## Data contract caution

- Current repository processor builds addresses under `/leap/{alias}/{hand}/...`. Hand text emitted by frame conversion is lowercase `left` / `right`.
- Position fields (`tx`, `ty`, `tz`) are **millimeters** in the existing code; do not silently treat them as meters inside TD.
- The existing per-scalar sends are separate UDP messages, which can arrive at slightly different moments; if bundling is introduced, test compatibility with the actual TouchDesigner OSC receivers.
- A single frame may contain no hands. Previously present hands may be reset to zero on disappearance in the legacy path. A zero position is not a robust validity signal; use optional explicit presence/validity channel when available.
- Incoming OSC often needs filtering/normalizing before driving a 3D cursor. Keep engine-level processing optional and transparent to avoid accidentally applying smoothing twice (once in bridge, once in TD).

## Troubleshooting sequence when output stops

1. **TouchDesigner**: Is OSC In DAT seeing any packets? Port correct? Another process already listening? Are channels reinterpreted?
2. **Hand Bridge OSC**: Latest `last_output_attempt` and send-error counter changing? Configuration destination unchanged?
3. **Processing**: Are frames consumed? Are channel filters on? Is current hand valid? Are zero messages being generated?
4. **Queue**: Is newest frame arriving to processing? Drops/high-water/age spiking?
5. **LeapC**: Are tracking frame events arriving from each subscribed device? Is service connected?
6. **USB/Hyperion**: Does Ultraleap's own visualizer still track? Are devices available to service?

Do not restart all processes as the first diagnostic step if logs/counters can identify the fault. Once evidence is captured, try targeted recovery and record what restored operation.

## Future optional channel version

If a v2 OSC schema is implemented, explicitly label `/handbridge/v2/...` or another reviewed prefix and document exact schema alongside original legacy scheme. Keep legacy default and opt-in selection, and provide TD mappings so existing projects are not broken.

## Current simulated smoke test

Build the SDK-free target from the root README. Run
`handbridge-engine --simulate --headless --port 9000 --scenario stall --duration-ms 5000`
using its full build path. This emits `dev1/left` and `dev2/right` scalar messages.
Camera A pauses at 200–900 ms: default pose channels zero once after the 500 ms
stale timeout, B continues, then A recovers. The host prints simulated metrics;
TouchDesigner In DAT/CHOP must independently confirm actual receipt.

Cloud tests verify decoded loopback UDP, not TouchDesigner. No `.toe` receiver
or physical Windows result is supplied yet. Optional orientation/velocity/normal
retain historical loss behavior and can hold last values; see the protocol.
The optional Qt controller and actual hardware adapter are not part of this build.
