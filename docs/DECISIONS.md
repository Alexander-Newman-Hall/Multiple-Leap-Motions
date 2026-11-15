# Architecture decision record (ADR) log

Status vocabulary: `Accepted`, `Proposed`, `Superseded`. Codex should append a short ADR for any decision that changes these constraints.

## ADR-001 — Native C++ engine / Windows first
**Status:** Accepted by product owner. **Decision:** C++20 / MSVC / Windows 10/11 x64. Preserve LeapC as tracking source. **Why:** Existing native codebase, direct SDK integration, reliable unattended hardware control. **Consequence:** Cloud should exercise mock source; real SDK validation on Windows PC.

## ADR-002 — OSC / UDP is primary TouchDesigner transport
**Status:** Accepted. **Decision:** Send OSC to localhost on configurable port; preserve legacy message schema by default. **Why:** TouchDesigner OSC In CHOP/DAT compatibility. **Consequence:** No mandatory HTTP/WebSockets/network service. Local GUI IPC is strictly separate.

## ADR-003 — Independent camera streams, no fusion v1
**Status:** Accepted. **Decision:** Do not implement joint fusion, blending, handoff, cross-camera identity stitching by default. **Why:** Reliability first, users can route individual cameras in TD. **Consequence:** Fixed-rig world transforms may be added without fusion; future work requires a new ADR and validation plan.

## ADR-004 — Headless engine + separate optional native control process
**Status:** Accepted. **Decision:** `handbridge-engine.exe` owns tracking/processing/OSC. `handbridge-control.exe` only configures and observes. **Why:** Closing UI must not interrupt installation. **Consequence:** Explicit local control IPC, centralized config validation and authoritative status.

## ADR-005 — Qt 6 Quick/QML for optional GUI
**Status:** Proposed default. **Decision:** Prefer Qt Quick/QML views with C++ models/controllers, no React/JS web stack. Qt Widgets/C++ may replace it only with a documented code-level justification. **Consequence:** Keep QtGui/QtQuick dependencies out of engine; evaluate Qt licensing and packaging.

## ADR-006 — Local IPC, no remote control
**Status:** Proposed default. **Decision:** `QLocalServer`/`QLocalSocket` using local OS IPC with same-user access and versioned commands. **Why:** Native, lightweight, no browser/network stack. **Consequence:** QtCore/Network headless dependency if selected; validate duplicate engine instance safety.

## ADR-007 — GitHub Actions + real Windows hardware gating
**Status:** Accepted. **Decision:** Code developed in cloud and automatically built/tested on Windows CI. Hardware support cannot be marked validated until physically tested with two cameras/TD on Windows. **Consequence:** Clear build/test evidence and handoff docs in every task.

## ADR-008 — Protect legacy code and docs
**Status:** Accepted. **Decision:** Start with audit, baseline and feature branch; preserve historical README and original source in Git history. **Consequence:** No massive unreviewed rewrite or silent compatibility break.

## ADR TEMPLATE

### ADR-XXX — Title
- **Status:** Proposed/Accepted/Superseded
- **Context:** Why this decision is required
- **Decision:** Exactly what changes
- **Alternatives:** Options considered
- **Consequences:** Product, technical, build and documentation effects
- **Evidence:** Source/test references and date

## ADR-009 — Add a tested vertical slice alongside the legacy application

**Status:** Accepted for Phase 0/1, 2026-11-15. **Context:** the live application
consumes frames from the UI event loop, CMake cannot configure without graphics,
and hardware SDK headers are unavailable in cloud. **Decision:** preserve legacy
source/build definition and README; compile existing processor/config/OSC with a
new small headless source/supervisor seam and two-camera mock. Use one owner thread
and one latest-frame mailbox per serial initially. **Consequence:** deterministic
CI and rollback without claiming the hardware adapter was replaced. Qt/IPC and
real polling remain later gates. Evidence: LEGACY_AUDIT and EngineRuntime tests.

## ADR-010 — Preserve oscpack serialization; check native UDP results

**Status:** Accepted, 2026-11-15. **Context:** inspected upstream oscpack Send
ignores native send return values on both platforms. **Decision:** retain its OSC
serializer and scalar packets, replace the sender socket wrapper with checked
nonblocking Windows/POSIX RAII sockets, and count OS errors. **Alternatives:**
patching vendored SDK-independent oscpack socket code adds dependency maintenance;
rewriting serialization adds parity risk. **Consequence:** honest send metrics;
OS acceptance still does not imply TouchDesigner receipt. Golden tests decode
actual UDP independently and a kernel permission error verifies failure reporting.

## ADR-011 — Repair legacy configuration before introducing v2

**Status:** Accepted, 2026-11-15. **Decision:** preserve keys and valid saved ports,
normalize observed alias wrappers, validate full candidates before applying, and
reject versioned examples until migration exists. The new host reads explicit
config without automatic saving. **Consequence:** no silent schema downgrade or
installation rewrite; legacy save is still non-atomic, v2/backup/presets deferred.
Evidence: CONFIG_AUDIT and configuration regression tests.
