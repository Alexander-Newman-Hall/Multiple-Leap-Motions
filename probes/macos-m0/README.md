# M0 standalone library probes

The Mac product scope is **Apple Silicon only, native arm64**. Historical Intel
measurements are reference evidence only; they cannot establish arm64 acceptance.
No Intel, Rosetta or universal-release requirement is planned.

These owned, SDK-free sources are isolated from the production build. Compilation,
linking and execution on Linux do not establish Apple libc++ support. Run each
command natively on the Apple Silicon Mac; retain errors and exit codes as well
as successful output. Confirm `uname -m` reports arm64 and reject translated execution.
Do not silently change the compiler, language level or deployment target to pass.

```sh
mkdir -p work/macos-m0
sw_vers
uname -m
system_profiler SPHardwareDataType # redact hardware serial/UUID before sharing
xcode-select -p
xcodebuild -version # may be unavailable with CLT only; retain that result
xcrun --sdk macosx --show-sdk-version
xcrun clang++ --version
cmake --version
printf '#include <version>\n' | xcrun clang++ -std=c++20 -dM -E -x c++ -
xcrun clang++ -arch arm64 -std=c++20 -pthread probes/macos-m0/threading.cpp -o work/macos-m0/threading
./work/macos-m0/threading
xcrun clang++ -arch arm64 -std=c++20 -pthread probes/macos-m0/snapshot.cpp -o work/macos-m0/snapshot
./work/macos-m0/snapshot
otool -L work/macos-m0/threading work/macos-m0/snapshot
```

Record `MACOSX_DEPLOYMENT_TARGET` (including unset), compiler target, SDK and
`_LIBCPP_VERSION` from the macro output. These are host probes, not a decision on
the application deployment minimum. Use an external timeout harness if a broken
library fails to terminate; a hang is a failure, never a pass.

Then inspect the **installed official Mac Hyperion 6.2 SDK**, using explicit paths
from that installation rather than guessing its layout. Record provenance/version,
licence/sample instructions, `ls -l` symlink targets, `file`, `lipo -info`, `otool -L`,
`nm -gU` exports and `shasum -a 256` for the actual header and resolved dylib. Keep
vendor files and private machine evidence in ignored `work/`. Do not use the
Windows extraction helper or commit vendor samples.

The owned `sdk-diagnostic.cpp` is isolated from production and uses only the
selected official `LeapC.h`, the standard library and Darwin `dlopen`. It resolves
twelve functions with signatures obtained from that header. It checks the client
and connected ServerLibrary independently for 6.2+ within 6.x, bounds inventory to
64 devices, rejects duplicate serials, reports counters/ages and zero-hand frames,
and releases subscriptions/devices/connection before unloading. It runs for five
seconds or until Ctrl-C; it does not implement production recovery or OSC.

```sh
xcrun clang++ -arch arm64 -std=c++20 -Wall -Wextra -pthread -I "/actual/sdk/include" \
  probes/macos-m0/sdk-diagnostic.cpp -o work/macos-m0/sdk-diagnostic
./work/macos-m0/sdk-diagnostic /absolute/path/to/libLeapC.dylib --load-only
./work/macos-m0/sdk-diagnostic /absolute/path/to/libLeapC.dylib
```

Exit codes: 0 accepted client (load-only) or observed accepted service connection;
2 usage/library/export failure; 3 no service connection event within five seconds;
4 unavailable/unsupported API version; 5 SDK connection/poll failure. A zero exit
never asserts that devices exist or frames arrived. Logs include device serials
when hardware is present; review/redact before sharing. Shutdown is orderly on
SIGINT/SIGTERM; the outer harness enforces a 15-second deadline and records a hang
as failure if SDK cleanup itself blocks.

`.github/workflows/macos-m0.yml` selects the native arm64 `macos-15` runner,
requires actual arm64 execution and retrieves only the official **Apple Silicon
6.2.0** package linked by the LMC2 download page. It verifies the pinned SHA-256,
records its signature and expands it without installing or starting a service.
Original feature probes are optional re-audits; their established library gaps
are not fixed by repetition. The arm64 application presets are
`macos-arm64-core` and `macos-arm64-control`.

`run-sdk.py` selects only the inspected canonical `LeapSDK/include/LeapC.h` and
`LeapSDK/lib/libLeapC.6.dylib`, rejects ambiguous matching roots, hashes both files,
inspects native architecture/exports and sample/terms filenames/hashes, builds the
owned client, checks a missing absolute path, loads the real library, then requires
exit 3 for the genuine no-service run. An unexpected live service fails this
particular no-service check and must be investigated rather than reclassified.
No vendor source/package/header/library is uploaded; artifacts contain diagnostic
text only. The Apple Silicon 6.2.0 package SHA-256 is pinned from the inspected
signed/notarized package. If hosted arm64 capacity is unavailable, record BLOCKED
or use a local Apple Silicon Mac; never substitute Intel results or a translated
process. Do not replace a failing official package with Windows files.

See [recorded evidence and limitations](../../docs/reports/MACOS_M0.md). CI cannot
establish the owner's Mac configuration, physical multi-device streaming, USB
recovery or TouchDesigner receipt. Physical two-camera tests remain NOT RUN.
