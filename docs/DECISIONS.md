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

## ADR-012 — User-approved native VST-style controller rebuild

**Status:** Accepted by explicit user request, 2026-11-15. **Context:** The user
requested a complete modern GUI and control over settings after reviewing the
headless baseline. **Decision:** implement Qt6 Quick/QML with C++ controllers as
a separate process, a QtCore/Network-only IPC host and typed engine settings. Use
the existing deterministic source until real SDK integration is available. Knobs,
toggles, profiles and output controls require engine validation/acknowledgements
and packet-level effects; visual state alone is not sufficient. **Consequence:**
Phase3 and a narrow processing/settings slice proceed before the physical Phase2
gate by user authorization. Hardware claims remain blocked, original ImGui source
and prior branches remain recoverable. No web frontend or window-owned tracking.

## ADR-013 — Engine-authoritative settings and native process lifetime

**Status:** Accepted, 2026-11-15. **Decision:** use strict typed schema2 with explicit
legacy import, CAS revisions and acknowledgement, independent source/OSC worker,
bounded same-user length-framed native IPC and QtQuick-only controller. Profiles
use real atomic/backup storage with monotonic saved snapshots. **Consequence:**
UI attach/kill/reopen and profile disk work cannot own/block tracking lifetime;
tests must observe actual UDP. Default Qt-free build and legacy source remain
available. No HTTP/WebSockets or process-owned GUI tracking loop.

## ADR-014 — Real VST-style controls with explicit processing scope

**Status:** Accepted, 2026-11-15. **Decision:** global positional filters/transforms
and14 channel controls, per-device identity/mute/hand routing, separate optional
gesture hysteresis channels. Default raw-mm legacy values stay unchanged. **Why:**
the user requested modern tactile controls that affect output, not cosmetic sliders.
**Consequence:** every exposed schema field maps to an engine behavior; advanced
per-channel overrides/all-bone OSC/v2 remain unimplemented and are not presented
as working controls. Coordinate/unit changes are explicit profile choices.

## ADR-015 — Explicit Hyperion v6 SDK and verified runtime versions

**Status:** Accepted by explicit user instruction,2026-11-15. Use the SDK extracted
from the official Hyperion 6.2.0 installer, with pinned installer/tool/file hashes.
Do not substitute the earlier Unreal-plugin5.30 SDK. Use the actual LeapGetVersion
declarations to require client and connected ServerLibrary versions6.2.0 or newer
within 6.x. A Connection event alone is insufficient to report healthy service.
The Windows loader resolves an absolute client DLL with safe search flags; its
lifetime encloses adapter handle cleanup. --check-leapc loads/queries/unloads
without connecting to the service or writing state. Keep SDK files out of Git and
the portable preview; the official installed Hyperion runtime is a prerequisite.
**Consequence:** compilation, export/version probes and injected lifecycle tests
are feasible without cameras. Physical USB/service/TouchDesigner acceptance is
still required; unknown installed versions are reported or rejected, never assumed.

## ADR-016 — Release portable deployment with an independent engine

**Status:** Accepted by explicit user request,2026-11-15. Package sibling native
engine/controller executables, unmodified dynamic Qt 6.8.3/QML/platform plugins,
permitted app-local Microsoft Release CRT, dependency notices and exact Qt
corresponding source. Do not package Debug CRT, SDK-only archives or camera
drivers. Record source/header/Qt identities and diagnostic hashes in a manifest.
Test the extracted ZIP with developer PATH/settings removed, actual Windows
plugin/module paths, rendered images and continuing UDP across GUI lifecycles.
**Consequence:** this is an unsigned developer preview with an external Hyperion
installation, not an installer or hardware-accepted release. Cleaned CI search
paths do not prove every consumer Windows10/11 machine or GPU configuration.

## ADR-017 — Owned worker thread and mutex-protected immutable snapshots

**Status:** Accepted by explicit user instruction, 2026-11-16; narrow M2 slice.
**Context:** M0 measured missing jthread/stop_token/stop_callback and atomic
shared_ptr specialization in Xcode 16.4 / Apple libc++ 190102, independently of
successful Intel Hyperion 6.2 client loading. **Decision:** keep C++20, Qt 6.8.3,
Hyperion 6.2+ within 6.x and the existing GUI. Replace only the IPC host's worker
with one owned std::thread, an atomic bool stop flag set under the command mutex,
condition-variable wake and join outside all locks. Join on constructor startup
failure before rethrowing, and reject commands after stop. Preserve the 32-command
bound and worker ownership of source/processing/OSC. Publish immutable shared
snapshots through SnapshotMailbox: lock only to copy/swap a shared pointer; build
JSON and retire old values outside that lock. Readers retain old snapshots safely.
**Alternatives:** experimental Apple library flags or a replacement standard
library change the toolchain contract; a platform-specific fork duplicates worker
lifecycle; moving work onto Qt's event loop breaks engine independence. **Effects:**
no IPC schema, OSC, config, GUI or SDK policy change. Stop/join remains owned by the
single control thread, idempotent on that thread. Full M2 protected Mac IPC and
physical M1 acceptance remain open; this is not a distributable Mac application.
**Evidence:** MACOS_M0 report; snapshot coherence/lifetime and worker startup/normal
shutdown regressions; actual verification is recorded in reports/MACOS_M2_CONCURRENCY.md.

## ADR-018 — Native Apple Silicon target and per-socket Darwin SIGPIPE handling

**Status:** Accepted by explicit user instruction, 2026-11-16. **Scope:** planned
Mac product is native Apple Silicon arm64 only. Intel builds, Rosetta execution
and universal releases are not requirements; historical Intel results are retained
only as history and never satisfy arm64 acceptance. Windows x64 is unchanged.
**Fix:** set SO_NOSIGPIPE on each Darwin OSC socket immediately after creation,
before connect/send. Check setsockopt failure through existing RAII cleanup and
error reporting. Keep send flags, serializers, Windows/Linux paths and process-wide
signal disposition unchanged. Failed sends must return/count errors rather than
kill the engine. A Darwin-only child test restores and unblocks default SIGPIPE,
then checks repeated failed sends and unchanged signal policy. No SIG_IGN workaround.
**Validation:** arm64-only presets and runner guards; actual application/diagnostic/
SDK binary slice checks; engine/GUI mock suites, native Cocoa GUI lifecycle smoke,
and pinned genuine Apple Silicon Hyperion 6.2 load/no-service checks. Universal
Qt dependency files may contain multiple slices; that is not an Intel product
build. Arm64 acceptance requires arm64 execution. Hosted capacity failure is a
blocker, not a reason to substitute Intel. Physical service/two-camera/USB/TD
acceptance and protected IPC remain separate gates. Evidence: reports/MACOS_ARM64.md.
