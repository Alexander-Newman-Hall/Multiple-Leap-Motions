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

The SDK-only diagnostic is **not yet prepared or built**: this execution environment
has no installed Mac SDK to establish its exact API/ABI or sample terms. Complete
it on the Mac using the installed header and explicit absolute dylib path. Require
bounded polling/runtime, client and connected ServerLibrary 6.2+ within 6.x checks,
distinct device serials, per-device counters/ages including zero-hand frames,
reported errors and cleanup on timeout/Ctrl-C. Test missing-library and genuine
no-service paths separately. Do not substitute the Windows production loader.
See [actual blocked evidence](../../docs/reports/MACOS_M0.md).
