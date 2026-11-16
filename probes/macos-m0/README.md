# M0 standalone library probes

These owned, SDK-free sources are isolated from the production build. Compilation,
linking and execution on Linux do not establish Apple libc++ support. Run each
command on the actual Mac; retain errors and exit codes as well as successful output.
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
xcrun clang++ -std=c++20 -pthread probes/macos-m0/threading.cpp -o work/macos-m0/threading
./work/macos-m0/threading
xcrun clang++ -std=c++20 -pthread probes/macos-m0/snapshot.cpp -o work/macos-m0/snapshot
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
xcrun clang++ -std=c++20 -Wall -Wextra -pthread -I "/actual/sdk/include" \
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

`.github/workflows/macos-m0.yml` selects `macos-15` (one actual runner architecture,
not a universal claim), runs the Apple probes independently, retrieves the official
architecture-specific 6.2.0 Mac package linked by the LMC2 download page, records
its SHA-256/signature and expands it without installing or starting a service.
`run-sdk.py` rejects ambiguous SDK layouts, hashes the actual header/resolved dylib,
inspects binary metadata/exports and sample/terms filenames/hashes, builds the
owned client, checks a missing absolute path, loads the real library, then requires
exit 3 for the genuine no-service run. An unexpected live service fails this
particular no-service check and must be investigated rather than reclassified.
No vendor source/package/header/library is uploaded; artifacts contain diagnostic
text only. Package hashes are evidence of the retrieved input; initial retrieval
uses official HTTPS plus Apple's package signature inspection, not a pre-existing
Mac checksum pin. Do not replace a failing official package with Windows files.

See [recorded evidence and limitations](../../docs/reports/MACOS_M0.md). CI cannot
establish the owner's Mac configuration, physical multi-device streaming, USB
recovery or TouchDesigner receipt. Physical two-camera tests remain NOT RUN.
