# macOS M0 — actual automated feasibility evidence

Date: 2026-11-16 (America/Los_Angeles). Branch:
`codex/handbridge-macos-feasibility`, from planning revision
`d104b87b4bdda248b12a90821f21a0742d479ce8`. Original feasibility work preserved
in `f4f60a5`; automated diagnostic first native-tested at `c9f337d`, ownership correction in
`1b29631`, and both package checksum pins in `cab1839`.
**M0 automated evidence collected with compiler/arm64 capacity blockers.
Intel offline SDK checks PASS. Physical two-camera tests NOT RUN.
M2 concurrency work is tracked separately; M3–M5 not started.**

## M0 final Intel evidence collected before M2

[SDK-only retry 43962640624](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43962640624),
source **e1fcd18**, completed **SUCCESS**, including text artifact upload.
This confirms the final diagnostic ownership correction and both checksum pins.
Same actual Intel environment: macOS 15.7.9 (24G830), Xcode 16.4/16F6, Apple Clang
17.0.0 (clang-1700.0.13.5), SDK 15.5, libc++ headers 190102, CMake 4.4.2.
Canonical header/dylib hashes match the Intel inventory below. Diagnostic compiled
and linked without warnings. Missing library exit 2, real client load exit 0
(**6.2.0**), five-second no-service run exit 3, zero devices/poll errors and clean
shutdown all passed. **ServerLibrary unavailable; no service/hardware acceptance.**
Original feature probes were deliberately not repeated; their measured failures
remain a host code issue, independent of successful SDK loading.

M0 automated evidence is now collected with a precise remaining arm64 allocation
blocker. The user explicitly authorized the small M2 concurrency compatibility
slice next, retaining C++20, Hyperion 6.2 and the GUI. M1 remains NOT RUN; full M2
(especially protected Mac IPC), M3 real loading, M4 packaging and M5 hardware
acceptance are not delivered by this result. Historical attempts below remain
traceable; the old pending repeat 43958755065 is resolved as a network failure,
then superseded by this successful retry.

## Actual Apple compiler results

[Run 43955399147](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43955399147),
source `4c48c617f56a2e4d5f4fcbe6103a22279385b26f`, completed **FAILURE**.

| Input | Measured value |
|---|---|
| Runner label | macos-15; hosted Apple Virtual Machine 1 / VirtualMac2,1 |
| Chip / architecture | Apple M1 (Virtual), arm64, 3 cores, 7 GB |
| macOS / Darwin | 15.8.2 (24H232) / 24.6.0 |
| Selected developer tools | /Applications/Xcode_16.4.app/Contents/Developer |
| Xcode / build | 16.4 / 16F6; not a separate CLT-only installation |
| Apple Clang | 17.0.0 (clang-1700.0.13.5), arm64-apple-darwin24.6.0 |
| libc++ headers | `_LIBCPP_VERSION=190102` |
| macOS SDK | 15.5 |
| CMake | 4.4.5 |
| MACOSX_DEPLOYMENT_TARGET | unset; no application minimum established |

Commands (full environment commands in probes/macos-m0/README.md and workflow):

```sh
python3 probes/macos-m0/run-features.py
# Independently, for each of threading and snapshot:
xcrun clang++ -std=c++20 -pthread probes/macos-m0/threading.cpp -o work/macos-m0/threading
xcrun clang++ -std=c++20 -pthread probes/macos-m0/snapshot.cpp -o work/macos-m0/snapshot
```

Both commands exited **1 during compilation**. Threading: no `std::jthread`,
`std::stop_token` or `std::stop_callback`. Snapshot: `_Atomic` cannot be applied to
non-trivially-copyable `std::shared_ptr<const int>`; atomic specialization absent.
Link/run checks consequently **NOT RUN** for both on this toolchain. No experimental
flags, alternate library or production compatibility shim was added to hide this.
The feature step has continue-on-error so SDK inspection can proceed; GitHub's
step conclusion is not the compiler result. The final job explicitly fails when
feature probes fail. M2 needs its planned compatibility decision; no such rewrite
belongs to this M0 task.

## Genuine Mac Hyperion 6.2 SDK inspection

Official URL linked on the LMC2 download page, rechecked in this task:
https://s3.eu-west-1.amazonaws.com/downloads.ultraleap.com/software/tracking-software/6.2.0/tracking-software-apple-silicon-6.2.0.pkg

Package SHA-256:
`9cad82a6282e322d2acd71f3746e50b472f3ed83bb5f9fa6f54d554f404c5531`.
The workflow now checks that digest before expansion. `pkgutil --check-signature`
reported **Developer ID Installer: ULTRALEAP LIMITED (6349LWZUED)**, signed with a
trusted timestamp, **trusted by the Apple notary service**. This describes the
vendor package, not signing/notarization of Hand Bridge.

`pkgutil --expand-full` inspected the package without installation or service
startup. SDK is under `Applications/Ultraleap Hand Tracking.app/Contents/LeapSDK`:

| File | Actual finding |
|---|---|
| include/LeapC.h | 138484 bytes; SHA-256 `40b2943f5be3271b98ad96de987f85396136af9522b2dbb7fdf5bbbd79e8265c` (confirmed independently on the Intel runner and from the pinned Apple Silicon package on Linux) |
| lib/libLeapC.dylib | symlink → libLeapC.6.dylib |
| lib/libLeapC.6.dylib | 7270080 bytes; Mach-O arm64 only; SHA-256 `843e3e3e4ba43bff40db3d8152175c1f8c9f5c7521b75d5e1bff62b83501e245` |
| install name | @rpath/libLeapC.6.dylib, compatibility 6.0.0, current 0.0.0; not an API version claim |
| exports | nm -gU lists the twelve required API functions, including LeapGetVersion and per-device subscribe/unsubscribe |

Important observed distinction: the package also includes CFFI library copies
and a control-panel plugin whose install name is `@rpath/libLeapC.5.dylib`.
The first diagnostic harness correctly rejected ambiguous selection before
loading anything. The corrected harness explicitly selects the canonical C SDK
include/lib directories, never the plugin or CFFI copy. No Gemini fallback.

Bundled README and sample CMake instructions were inspected, not copied into the
repository. README SHA-256 `57e1abb75847919eb2a556a36723a2cf895e2b1d9d5630fa40b482261eca125a`;
sample CMake SHA-256 `2b4c3f0671a8c91e6507bce66f273ef01b6d09e18d49f9d7c43b4c032daedca8`.
README requires CMake 3.16.3+, uses the app's Contents/LeapSDK/samples as the Mac
source directory and documents configure then cmake --build. Its generic usage
section contains older package-version wording; the actual sample CMake requires
LeapSDK **6**, links LeapSDK::LeapC and includes MultiDeviceSample. Those sample
names establish no Mac multi-camera runtime support. Our owned thin client copies
no sample implementation; SDK licence/notices remain external. Samples were not
built or redistributed. No vendor SDK/header/dylib/package is committed/uploaded.

## Bounded SDK diagnostic and execution status

Owned `probes/macos-m0/sdk-diagnostic.cpp`: exact official header signatures,
absolute-path RTLD_NOW/RTLD_LOCAL load, twelve required symbols, independent client
and connected ServerLibrary 6.2+ within 6.x gates, five-second deadline, ten-ms poll,
64-device cap, bounded error logging, distinct serial validation, per-device frame
counters/ages/hand counts, and handle cleanup before unload. Empty-hand frames
advance the frame counter. SIGINT/SIGTERM request orderly exit. The outer harness
enforces a 15-second deadline, including cleanup. No engine, GUI, config or OSC use.

```sh
python3 probes/macos-m0/run-sdk.py
# Actual compile command uses the inspected absolute include path:
xcrun clang++ -std=c++20 -Wall -Wextra -pthread -I "$SDK/include" \
  probes/macos-m0/sdk-diagnostic.cpp -o work/macos-m0/sdk-diagnostic
# Harness then checks missing absolute path (exit 2), load-only (exit 0),
# and genuine no-service startup/cleanup (exit 3), without installing a service.
```

[Intel run 43957514963](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43957514963),
source `c9f337d`, completed with SDK diagnostic checks **PASS** and overall job
**FAILURE solely from both C++20 feature probes**. Compiler errors match the arm64
run. Diagnostic itself compiled/linked with Apple Clang; one missing-aggregate-field
warning subsequently removed by explicit Session construction in `1b29631`.
That commit also allocates the owning device record before opening its SDK handle,
so allocation failure cannot orphan a handle. A final exact-source repeat at
`cab1839` is [run 43958755065](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43958755065)
completed 2026-11-16: the host inspection ran; package download failed with
`curl: (56) Recv failure: Connection reset by peer`. Diagnostic **NOT RUN**,
not a library-loading failure. Artifact upload also failed with ETIMEDOUT.
Actions logs retained both failures. The retry now uses curl --retry-all-errors
with bounded connection/runtime limits and skips known feature probes by default.
A workflow_dispatch boolean retains an explicit re-audit option. Existing successful
Intel SDK evidence at c9f337d is unchanged. A fresh SDK-only retry will record its
own exact source/run; Apple Silicon remains separately blocked on allocation.

Intel machine/toolchain: `macos-15-intel`, x86_64, system_profiler reports Mac mini /
Macmini6,2, processor name **Unknown** (do not infer a physical chip model),
4 cores / 14 GB; macOS **15.7.9 (24G830)**, Darwin 24.6.0, Xcode **16.4 (16F6)**,
Apple Clang **17.0.0 (clang-1700.0.13.5)** targeting x86_64-apple-darwin24.6.0,
libc++ **190102**, macOS SDK **15.5**, CMake **4.4.2**, deployment target unset.

Its separate official URL uses `tracking-software-apple-intel-6.2.0.pkg` in the
same official directory. Package SHA-256
`cb6279bacbda12b0e2db2eda5f1e1fd1988f77f568680150e63aee474d6552bc`;
trusted ULTRALEAP LIMITED installer signature and Apple notarization confirmed.
The workflow now pins this independently observed hash too. Canonical dylib is
8457936 bytes, **x86_64 only**, same symlink/install-name convention; SHA-256
`20d3ca486d8b3f68bd8f817821c9a8c14cdb403d2932d0f36f5e19b914dfa77b`.
The header hash is the same Mac hash shown above; this is observed, not inferred.

Actual retained output:

```text
missing absolute library path: diagnostic error: dlopen(...) ... no such file
exit= 2 expected= 2
client result=0 version=6.2.0
exit= 0 expected= 0
client result=0 version=6.2.0
summary ever_connected=0 connected=0 devices=0 poll_errors=0 interrupted=0
service version=unavailable (no connection event within 5s)
exit= 3 expected= 3
M0 OFFLINE CHECKS PASS; PHYSICAL TWO-CAMERA TESTS NOT RUN
```

The no-service loop lasted approximately five seconds and completed cleanup within
the 15-second harness deadline. No service was installed, started or stopped.
ServerLibrary was **unavailable**, not assumed 6.2 from the client/package. The
normal timeout path exited cleanly; Ctrl-C/signal cleanup has code but was not
separately exercised in this task. Device serial/counter branches have not seen
real devices; do not declare them hardware tested.

[Corrected arm64 run 43955834798](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43955834798)
failed **before any job steps executed** after approximately 15 minutes queued.
GitHub check annotations: “The job was not acquired by Runner of type hosted even
after multiple attempts” and “Due to capacity constraints, jobs targeting macOS
arm64 runners may experience longer queue times.” Thus canonical arm64 diagnostic
compile/load/no-service checks are **BLOCKED by hosted runner allocation**; the
first arm64 run did inspect compiler/SDK, but rejected ambiguous diagnostic input.
The macOS 26 and latest-source arm64 capacity attempts were cancelled while still
queued after this explicit failure. They supply no macOS 26/new toolchain evidence.

| Check | Actual result |
|---|---|
| Apple C++20 threading and snapshot probes, arm64 and Intel | FAIL compilation on the measured Xcode 16.4 toolchains; link/run NOT RUN |
| Intel diagnostic compile/link | PASS against genuine Mac header |
| Intel missing-library / load-only / no-service | PASS: expected exits 2 / 0 / 3 |
| Intel ClientLibrary API | 6.2.0, all twelve required exports resolved |
| Connected ServerLibrary API | unavailable, no service connection; NOT VERIFIED |
| Canonical arm64 diagnostic compile/load/no-service | BLOCKED: hosted runner allocation failure |
| Physical serials / per-device frames / no-hands / recovery / TD | NOT RUN |

Text-only artifacts are attached to the executed runs with 14-day retention; durable
machine versions, hashes and key output are recorded here. SDK files are excluded.
Earlier superseded jobs were cancelled, not passed. No universal binary claim.

## Local sanity and preserved product

The initial environment was Linux x86_64 kernel 6.18.57, Debian GCC 14.2.0-19;
CMake, sw_vers, xcrun, xcodebuild and Mac SDK were absent in searched locations.
Both SDK-free probes compiled/linked and exited 0 under a five-second timeout on
Linux. After explicitly retrieving the pinned **Mac** package into ignored work/,
GCC compiled/linked the thin diagnostic against its actual Mac header; the initial aggregate warning was removed and final Linux recompilation has
no warnings/errors. A missing-library sanity run exited 2.
These Linux results establish no Apple/library/service/camera success.
`python3 -m py_compile` for both harnesses and `git diff --check` passed.

No production Windows source, GUI, loader, packaging, CMake presets, OSC bytes or
configuration changed. Hyperion 6.2+ within 6.x remains mandatory. Existing
[core CI](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43953539333),
[native controller CI](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43953539590)
and [Windows Release CI](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/runs/43953539477)
passed at preserved checkpoint f4f60a5. Windows physical acceptance remains open.

## Remaining M0 evidence

Canonical arm64 diagnostic build/load/no-service execution remains BLOCKED on
hosted runner allocation. Collect it on an available arm64 runner or the owner's
Mac, using the pinned official package and recording actual client/service
versions. Intel SDK evidence is complete at e1fcd18; original host-feature failures
are a separate code finding. The user-authorized M2 concurrency slice is tracked
in [its own report](MACOS_M2_CONCURRENCY.md). Physical M1 and all two-camera,
service recovery and TouchDesigner acceptance remain NOT RUN.
