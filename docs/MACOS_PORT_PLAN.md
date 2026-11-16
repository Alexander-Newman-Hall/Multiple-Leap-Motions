# Hand Bridge macOS port plan

Status: **planning only**. No Mac build, native SDK load, camera test or Mac
package is delivered by this document. User authorized this plan on 2026-11-16.

Execution checkpoint: M0 automated probes and the SDK-only diagnostic are on
`codex/handbridge-macos-feasibility`; [actual results and remaining blockers](reports/MACOS_M0.md)
include Apple compiler failures and passing Intel offline checks. This does not
advance M1 or establish physical two-camera support. The milestones and exact
first-task prompt below remain the scope contract.

Planning branch: `codex/handbridge-macos-plan`, based on Windows handoff
`2c1ce8e72980c0ef4d59efa474e19df30c2666a2`. Validated Windows application/package
source remains `c8596f20b35bb0e4d09446933c543f689417f36c`; see
[the Windows evidence](reports/HYPERION6_RELEASE.md). Preserve that branch, PR #3,
the original branches and Windows CI. Create a separate implementation branch
from this plan; do not overwrite or restart the project from master.

## Goal and limits

Port the existing native C++20 engine and Qt Quick controller to a Mac, retaining
independent camera streams, localhost OSC/TouchDesigner compatibility, profiles,
processing, recovery and GUI-independent engine lifetime. Reuse the current GUI.
Keep Hyperion **6.2.0 or newer within 6.x**, with actual client and connected
service API version checks. No Gemini downgrade, fusion, cross-camera identity,
network relay, web UI, elevated background service or unrelated feature expansion.

Target the owner's actual Mac architecture first. Apple Silicon is the proposed
first target if no machine has been selected; Intel support is a separate tested
matrix entry. Do not promise a universal binary until both slices and all native
dependencies have been inspected and tested.

## Evidence and unresolved questions

Checked on 2026-11-16; recheck before implementation:

- [Official LMC2 downloads](https://www.ultraleap.com/downloads/leap-motion-controller-2/)
  list Hyperion 6.2.0 for Apple Silicon and Intel, with SDK and tracking service.
  Ultraleap's listed macOS minimum is 11; that is not Hand Bridge's application minimum.
- [Ultraleap's multiple-device guidance](https://docs.ultraleap.com/hand-tracking/integration-tips-resources.html#multiple-devices)
  still describes simultaneous multi-device support as Windows-only, using older
  5.x wording. The generic [LeapC multi-device example](https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html)
  is not a guarantee of Mac service support. **Two real LMC2 devices on one Mac
  remain unverified.** Neither technical impossibility nor successful support
  follows from the available documentation.
- [Qt 6.8 supported platforms](https://doc.qt.io/qt-6.8/supported-platforms.html)
  list macOS 12+ deployment, Xcode 15+/macOS 14 SDK build inputs, arm64 and x86_64.
  Keep the current Qt 6.8.3 pin unless a separate decision justifies changing it.
  The final application minimum must also satisfy the selected C++ standard library
  and SDK; do not advertise macOS 11 merely because Ultraleap does.
- [libc++ C++20 status](https://libcxx.llvm.org/Status/Cxx20.html) distinguishes
  language support from library support. Joining threads/stop tokens and atomic
  shared pointers need compile **and link** checks on the actual Apple toolchain.
  Do not equate upstream LLVM versions with Apple's version numbering.
- [Qt 6.8 QLocalServer](https://doc.qt.io/qt-6.8/qlocalserver.html)
  documents that macOS ignores Unix-domain socket permission flags. Existing
  `UserAccessOption` alone is not a sufficient Mac same-user access guarantee.

## Code-level starting points

Line references are to the preserved `2c1ce8e` baseline, not promises that files
already support macOS.

| Existing component | Preserve / planned work |
|---|---|
| `src/tracking/LeapCTrackingSource.cpp`, `LeapCApi.hpp` | Preserve shared API types, state machine, serial routing, frame copies and cleanup; compile against the actual **Mac** header. |
| `src/tracking/LeapCLibrary.cpp:25`, `:78`, `:109` | Windows paths/LoadLibrary and an explicit non-Windows rejection; add a Mac loader without changing the Windows loader contract. |
| `cmake/LeapC.cmake:24`, `CMakePresets.json` | Current SDK target treats non-Windows as header-test-only; add explicit Mac platform/architecture/toolchain configuration. |
| `src/engine/qt_engine_main.cpp:119`, `:296`, `:301` | `std::jthread`, `std::stop_token`, atomic shared snapshots; probe Apple libc++ before designing compatibility changes. |
| `src/engine/qt_engine_main.cpp:313`, `src/ipc/Framing.hpp:15`, `src/control/Controller.cpp:114` | Map server/client tokens into the same secure Mac endpoint path; preserve protocol framing, limits and bounded telemetry. |
| `src/engine/qt_engine_main.cpp:57`, `src/core/ConfigManager.cpp:18` | Qt host already uses QStandardPaths; review explicit config/state paths and legacy non-Windows fallback without introducing a schema migration. |
| `cmake/NativeControl.cmake:12`, `src/control/Controller.cpp:359` | Add native app bundling; retain the engine beside the controller executable so detached launch remains valid. |
| `CMakeLists.txt:33`, `src/pipeline/04_OscSender.cpp` | Existing POSIX UDP path is reusable; verify Darwin error behavior and exact OSC bytes rather than assuming Linux results apply. |
| `scripts/fetch-leapc-sdk.py:151`, `cmake/WindowsDeploy.cmake` | Windows SDK extraction and Windows packaging are not Mac dependency/deployment tools. Keep them functional and add separate Mac setup. |

## Milestones and acceptance gates

Each milestone is a small reviewable task/PR. Update CURRENT_STATE and
SESSION_HANDOFF after each. Do not turn this document into a single rewrite.

### M0 — Mac environment, compiler and SDK probe

Run on a real Mac execution environment. Record chip/architecture, macOS build,
Xcode/CLT, Apple Clang/libc++ and CMake versions. Inspect the installed official
Mac Hyperion 6.2 SDK: actual header, dylib names/symlinks, architecture slices,
exports, hashes and bundled sample build instructions. Do not infer those from
the Windows installer or download proprietary files into Git.

Compile/link tiny probes for the host's exact C++20 threading/snapshot features.
Prepare a minimal console diagnostic using the installed SDK sample in place or
our own thin client; do not copy vendor samples into the repository without
checking their terms. It must report actual client/service versions, device serials,
per-device frame counters/ages and errors with bounded polling and clean exit.
Build and missing-library/no-service checks must work without connected cameras.

**Gate:** reproducible Mac probe commands and real toolchain/SDK evidence, or a
precise BLOCKED report. A successful compile or client-library load does not prove
that two cameras work. No production GUI/loader rewrite in this first task.

### M1 — Physical two-device feasibility

Use the M0 probe and two actual LMC2 devices with the official Mac service:

1. Confirm accepted client and ServerLibrary versions independently.
2. Identify two distinct serials and receive simultaneous, independent streams.
3. Observe at least two minutes of both frame counters advancing; distinguish
   healthy no-hands frames from missing frames. Record actual rates, not a target claim.
4. Unplug A: A's freshness changes while B continues. Replug A and record whether
   enumeration/subscription/frame delivery resumes; repeat for B and reverse ports.
5. Record Mac model, ports/hub/cables, service logs, CPU observations and any faults.

**Gate:** two independent streams demonstrated, or FAILED/BLOCKED with evidence.
Two USB entries or two names in a control-panel selector are not proof of
simultaneous tracking; retain per-device frame evidence.
If the Mac service refuses the second stream, stop the production two-camera port
and resolve support with current vendor evidence; do not hide it with simulation,
camera switching, fusion or a second PC. Intermittent recovery failures are blockers
for release even if basic simultaneous tracking succeeds.

M0 and SDK-free preparation can proceed without cameras. M1 remains pending until
the physical actions occur; full two-camera integration/packaging should not be
prioritized ahead of resolving this feasibility gate.

### M2 — SDK-free native Mac build, portability and protected IPC

Add architecture-specific Mac CMake presets and a pinned available macOS CI runner.
Choose the deployment target from actual dependencies/toolchain tests, not the
Ultraleap minimum alone. Build the Qt-free host and optional native controller;
run core/settings/processing/OSC goldens, profile, model, IPC and GUI-lifecycle tests.

If required C++ library features are unavailable, make a short ADR for the smallest
owned-thread/stop-flag and mutex-protected immutable-snapshot implementation.
Retain C++20, bounded queues, exception-safe stop/wake/join and the independent
worker. Never put SDK polling or OSC on the GUI/event loop. Test shutdown and
concurrent snapshot access; keep Windows/Linux regression jobs green.

Resolve local IPC before treating a Mac mock controller as safe to distribute:
map the existing validated endpoint token into a short private per-user directory
with owner-only directory access (0700), reject unsafe owner/symlink locations,
and account for Darwin's Unix socket path-length limit in UTF-8 bytes. Both client
and server must use the same mapping; preserve the CLI token and protocol version.
Verify same-user attachment, stale cleanup, duplicate-host exclusion and different-user
rejection where the controlled test environment permits it. Record an unexecuted
cross-user test explicitly; `UserAccessOption` is not substitute evidence on Mac.

**Gate:** native Mac compilation/mock suites and access/lifecycle checks pass.
Cloud/macOS CI status must be labelled **BUILD/MOCK TESTED**, not hardware tested.

### M3 — Real Mac engine integration

After M1 and M2, implement Mac dynamic loading (`dlopen`/`dlsym`/`dlclose`) behind
the existing library ownership seam. Begin with an explicit absolute SDK path;
add inspected installation discovery later. Check native architecture and all
required exports; query actual ClientLibrary/ServerLibrary versions. Preserve
handle cleanup before unload and avoid bare-name/CWD library search.

Do not fabricate Windows-style file-version metadata on Mac; expose API version
separately and mark unavailable platform metadata honestly. Keep serial/alias,
frame ownership, native IDs, freshness, filters and source state machine shared.
Audit profile paths, Unicode/space paths, read-only bundles, diagnostics redaction,
signals and bounded shutdown. Preserve legacy JSON imports and OSC schema/defaults.

Test missing/wrong-architecture/unsupported libraries and genuine no-service client
startup separately from real tracking. On the physical Mac, verify both cameras
headlessly into a real UDP receiver and TouchDesigner OSC In DAT/CHOP. No GUI is
required for tracking or output.

**Gate:** actual Mac two-device engine → receiver observations recorded, plus
shared Windows/Linux/Mac tests green. Sender success alone is not receiver receipt.

### M4 — Existing GUI and native application packaging

Reuse the current five-page QML/controller design and applied/saved semantics.
Verify native rendering, numeric entry/knobs, scrolling/Retina scaling, profiles,
startup preflight, error text and attach/close/kill/reopen with continuing OSC.

Package a proposed `HandBridge.app` with controller and separate headless engine
under `Contents/MacOS`, shared Qt dependencies under `Contents/Frameworks`, and
required QML/plugins/resources. Validate the layout before fixing it as a contract.
Deploy both executables with Qt's [Mac deployment tools](https://doc.qt.io/qt-6.8/macos-deployment.html),
including the helper via `macdeployqt -executable=.../Contents/MacOS/handbridge-engine`,
inspect architectures/install names/rpaths, and test after moving/extracting the
bundle with developer PATH/DYLD/Qt search variables removed. Test the Cocoa GUI
plugin; an offscreen render alone is insufficient for native UI acceptance.

Keep Hyperion/SDK external under the established licence boundary. Include notices
and the correct corresponding Qt source. Never copy Windows CRT/DLLs into a Mac
package. Record source revision, clean/dirty state, architectures, actual dependency
versions and checksums. No universal claim from one architecture's build.

An unsigned/ad-hoc developer preview and a publicly signed/notarized release are
different deliverables. Plan nested helper/framework signing, Gatekeeper and
notarization using the owner's authorised Apple credentials only when available;
never commit signing keys or disable system security as deployment policy.
Test external SDK loading under that final hardened-runtime/library-validation
policy too; document any required per-app entitlement in an ADR rather than
assuming an unsigned development load proves the signed application will work.

**Gate:** extracted native bundle works without a Qt developer install, packaged
helper remains independent, and signing/notarization status is explicit.

### M5 — Mac recovery and release acceptance

On each supported architecture, perform no-hands, both-device tracking, ten
unplug/replug cycles, controlled service restart, app reboot, sleep/wake, GUI crash/
reopen, profile restore and actual TouchDesigner checks. Start with a documented
one-hour two-camera soak, then an eight-hour deployment soak before a production
claim; capture frame ages/drops, recovery failures, CPU, memory and output errors.
Do not restart a shared tracking service or change system power policy without
appropriate local authorization.

**Gate:** actual PASS/FAIL/BLOCKED results per case; current Windows release remains
available and its regression checks pass. Document supported chip/macOS/SDK versions
and known failures; no generic “all Macs supported” claim.

## Evidence vocabulary and handoffs

| Evidence | What it establishes |
|---|---|
| Cloud or macOS CI compile/mock suites | Specified source builds; simulated processing, OSC and lifecycle behavior. |
| Actual Mac dylib preflight/offline host | SDK loading/version/symbols and bounded no-service startup/cleanup only. |
| Physical Mac, two USB devices | Only the measured discovery/stream/recovery cases actually exercised. |
| Receiver-side TouchDesigner observation | Receipt and values for the tested configuration. |
| Packaged-app smoke / signing checks | The tested bundle layout/runtime/security-policy behavior; not camera reliability. |

Keep Windows physical acceptance open too: its CI results did not test real cameras.
Use separate Mac result records so platform claims cannot be mixed. Each handoff
must contain commit/branch, exact machine/toolchain/SDK versions, commands/results,
physical actions NOT RUN, limitations and one next task. No tests are run by this
planning-only change; validate document links and preserve the tested source.

## Recommended next chat and exact first task

Use a fresh Codex chat **opened on the local Mac checkout**. A new cloud chat can
prepare code/CI but does not gain access to your Mac or USB cameras. The repository
plan and handoff carry context; no repeat audit or GUI redesign is needed.

Paste this into that chat:

> Repository: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions.
> Start from the latest `codex/handbridge-macos-plan` branch, preserve local edits,
> then create `codex/handbridge-macos-feasibility`. Read AGENTS.md, PROJECT_SPEC.md,
> PLANS.md, CURRENT_STATE, SESSION_HANDOFF, docs/MACOS_PORT_PLAN.md and relevant
> SDK/OSC/architecture/testing docs. Implement **M0 only**: inspect this actual Mac
> and official Hyperion 6.2 SDK, run compile/link probes for the host's C++20 library
> features, and prepare/build a bounded SDK-only diagnostic for versions, distinct
> device serials and per-device frames. Use exact installed Mac headers/dylib and
> official docs; never the Windows SDK extraction helper or a Gemini fallback.
> Preserve the working Windows product, GUI, legacy OSC and config. Do not begin
> production loader/GUI/packaging changes in this task. Run real probe checks if
> the environment allows them; record missing SDK/hardware as BLOCKED/NOT RUN.
> If both controllers are already available, collect initial M1 observations with
> the user's physical actions, without declaring the full hardware gate complete.
> Update current state and handoff with exact versions, commands, results and one
> next task. No Mac/two-camera success may be inferred from compilation or mocks.
