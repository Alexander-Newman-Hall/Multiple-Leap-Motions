# Native Apple Silicon arm64 scope and SIGPIPE validation — 2026-11-16

Branch: `codex/handbridge-macos-feasibility`, starting at 48f2e17. User scope now
supersedes the earlier Intel test plan: **native Apple Silicon arm64 only**. No
Intel product build, Rosetta execution or universal release requirement. Existing
Intel reports are historical diagnostics, never arm64 acceptance. Windows x64,
C++20, Qt 6.8.3, Hyperion 6.2+ within 6.x and legacy OSC/config remain preserved.

## Implementation

`src/pipeline/04_OscSender.cpp` enables Darwin SO_NOSIGPIPE per socket immediately
after socket creation and before connect. Failure uses the existing checked error
and RAII cleanup path. Send errors remain false returns/counters, not a global
signal-policy change. Windows/Linux code and OSC bytes are unchanged.
The existing broadcast permission failure case stays enabled. A new Darwin-only
GTest child explicitly defaults/unblocks SIGPIPE, checks three failed sends,
zero accepted packets, three errors, nonempty error text and unchanged default
signal disposition before/after socket cleanup. See ADR-018.

## Arm64-only validation path

- Presets `macos-arm64-core` and `macos-arm64-control` select only arm64. Intel
  presets are removed. Deployment target 12.0 remains a CI configuration, not an
  application minimum acceptance claim.
- Arm64 workflow requires native uname/Python architecture and rejects translation.
  Engine/controller, SDK dylib and diagnostic slices must be exactly arm64.
- Genuine `tracking-software-apple-silicon-6.2.0.pkg` is verified by pinned SHA-256
  `9cad82a6282e322d2acd71f3746e50b472f3ed83bb5f9fa6f54d554f404c5531`, plus actual
  installer signature inspection, and expanded without installing/starting a service.
  The owned thin diagnostic checks actual client and connected service version
  availability independently; no proprietary SDK files are committed/uploaded.
- Engine/GUI mock suites run in full, including legacy OSC, profiles, snapshot/
  worker, control model, UI and GUI/engine lifetime cases. If SDK retrieval passes,
  the existing adapter also compiles against the exact Mac header and runs its
  injected-API tests. Production Mac runtime loading remains a later M3 gate.
- A separate native Cocoa smoke checks rendered GUI captures, close/kill/reopen and
  continuing decoded simulated UDP from the independent engine. Offscreen Qt tests
  alone do not establish native Cocoa rendering.
- Every suite remains in the final failure gate; no SIGPIPE failure is suppressed
  by skipping a test or reclassifying overall success. Historical compiler probes
  are not repeated; the production replacement itself is compiled/executed.

## Actual results at this checkpoint

Local environment is Linux x86_64, GCC 14.2 / Qt 6.8.3 / CMake 3.31.6. The sender
and test changes built, and focused legacy OSC regressions **8/8 PASS**, 0.05 s.
This does not compile or exercise the Darwin branch or establish arm64 acceptance.
`python3 -m py_compile probes/macos-m0/run-sdk.py`, `bash -n` SDK fetch helper and
`git diff --check` passed. Full local native suite **64/64 PASS**, no skips/failures, **22.49 s**. Windows
regressions and native arm64 CI results are pending; none is inferred from Intel history.

At source **fae90ed**, existing Windows/Linux core/native controller jobs and
Windows Release/package CI all completed **PASS**:
[core 43971198928](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43971198928),
[native 43971198982](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43971198982),
[Windows Release 43971198934](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43971198934).
These are regression evidence only, not arm64 validation. Workflow YAML and preset
JSON parsed successfully; Python smoke/SDK scripts compile and SDK shell syntax
passes. No native GUI test was executed on Linux.

Native-only attempts at the same source:
[macos-15 arm64 43971199255](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43971199255),
[macos-26 arm64 43972156771](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43972156771).
Both currently queue without assigned runners or job-step execution. Await actual
outcomes/annotations; do not label queued checks successful or infer an SDK issue.

There is no local Apple Silicon execution environment attached: `uname -sm` is
Linux x86_64. If hosted arm64 cannot acquire a runner, record its actual job
annotation/status as BLOCKED, leaving engine/GUI/SDK arm64 checks NOT RUN. Do not
fall back to an Intel runner or Rosetta. The same commands can be executed on an
owner-authorized local Apple Silicon Mac with its official SDK.

## Open gates

Running Mac service and accepted ServerLibrary, physical two-camera independent
streams, no-hands, recovery, sleep/wake, soak and TouchDesigner receipt **NOT RUN**.
SDK/client load, mock frames, Cocoa rendering and compilation cannot pass those
physical gates. Protected same-user Mac IPC and packaging/signing remain open;
no Mac release is distributed by this work.
