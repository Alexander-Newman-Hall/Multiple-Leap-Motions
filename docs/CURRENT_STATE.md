# Current state — OSC checkpoint, 2026-11-15

Branch: `codex/handbridge-phase0-reliability`; base `handbridge-v2`
`5f547181f6d512ab6a8d2146022227e26cec48d2`. Audit commit: `350249f`; configuration repair: `36c8f9b`.
Original remote `master` remains `054e578525a5d5202ea8f6eb417ce55ded7d61b8`.

Phase 0 audit/inventory complete: [LEGACY_AUDIT](LEGACY_AUDIT.md). Configuration
baseline repaired: explicit 127.0.0.1:9000 and channel defaults, canonical alias
save/load including old wrappers, transactional validation and relative save fix.
[CONFIG_SCHEMA](CONFIG_SCHEMA.md) describes implemented legacy support versus
proposed v2. Direct save is not atomic; new host will not auto-save.

CLOUD: GCC14.2 C++20 direct config/alias build **17/17 PASS**, no skips, test XML
`work/config-tests.xml` (ignored local evidence). Source files: ConfigManager,
DeviceAliasManager, two test files, working legacy example and config audit report.
Windows default-path/filesystem behavior remains UNTESTED ON WINDOWS.

Integrated SDK-free CMake, scalar OSC fixtures/checked native sender, and mock
headless runtime are in progress as separate commits. WINDOWS CI NOT RUN at this
checkpoint. Cameras/Hyperion/USB/TouchDesigner are UNTESTED ON HARDWARE.
Reported field stall root cause remains unknown.

Next task in this session: commit/test OSC baseline, then complete the mock
reliability slice and record integrated results and single Windows handoff task.

OSC checkpoint: retained processor/oscpack serializer; native nonblocking checked
UDP socket, synchronized target/buffer state, packet/error/byte timestamps,
bounded logs and optional UI callback. Removed AppCore scalar logs. CLOUD actual
loopback/kernel tests: **8/8 OSC tests PASS**; exact 23/39 live and 28/34 loss
fixtures. Physical TouchDesigner/Windows remain unverified. See OSC_AUDIT.
