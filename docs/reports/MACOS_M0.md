# macOS M0 attempt — 2026-11-16

**M0 BLOCKED; SDK diagnostic incomplete. M1 NOT RUN.** This is a cloud Linux
checkout, not the owner's Mac. No Mac build, dylib load, service version query,
camera observation or Mac missing-library/no-service test was performed.

Branch: `codex/handbridge-macos-feasibility`, created from latest fetched planning
branch commit `d104b87b4bdda248b12a90821f21a0742d479ce8`; initial checkout clean.
This report and the probes are uncommitted changes atop that revision.

## Actual environment and checks

- `uname -a`: Linux 2082065c6b14 6.18.57 #1 SMP Tue Oct 20 21:56:04 UTC 2026
  x86_64 GNU/Linux. This is not evidence of the owner's Mac chip or OS.
- `c++ --version`: Debian GCC 14.2.0-19, 14.2.0; not Apple Clang/libc++.
- `cmake --version`: command not found.
- `command -v sw_vers xcrun xcodebuild`: exit 1, no tools found.
- `rg --files /workspace /opt /usr/local/include | rg
  '(LeapC\.h|libLeapC.*dylib)$'`: exit 1, no matching files in these searched
  locations. No claim of a whole-machine SDK search or knowledge of the Mac install.
- Each standalone source compiled and linked with `c++ -std=c++20 -pthread
  probes/macos-m0/<name>.cpp -o work/macos-m0/<name>`: exit 0 for `threading`
  and `snapshot`. Each `timeout 5s work/macos-m0/<name>`: exit 0.
  **Linux probe sanity checks only**, no production suite or mock camera test.
- `git diff --check`: exit 0.

## Missing evidence

| M0 requirement | Result |
|---|---|
| Mac model/chip, macOS build, Xcode/CLT, Apple Clang/libc++, CMake | BLOCKED: no Mac execution environment |
| Installed official Mac header/dylib, symlinks, slices, exports, hashes, sample terms/instructions | BLOCKED: no installed Mac SDK available |
| Apple C++20 compile/link/run results | NOT RUN; Linux results cannot replace these |
| SDK-only bounded diagnostic build and missing-library/no-service checks | BLOCKED; diagnostic not implemented against uninspected SDK |
| Actual client and connected ServerLibrary API versions | NOT RUN |
| Distinct camera serials, independent counters/ages, physical actions | NOT RUN; M1 remains pending |

No SDK downloaded, copied, redistributed or substituted with Windows files.
No production loader/GUI/CMake preset/packaging/OSC/configuration changes made.
Windows Hyperion 6.2+ within 6.x policy and historical evidence remain unchanged;
Windows physical acceptance remains open. No two-camera conclusion follows here.

## Single next task

Resume M0 on the local Mac checkout of this implementation branch. Follow
`probes/macos-m0/README.md`, retain actual toolchain/SDK evidence, then implement
and build the bounded SDK-only diagnostic against that installed official header
and dylib. Exercise missing-library and no-service paths without changing a shared
tracking service. Record real versions/results and keep M1 pending unless physical
observations are actually collected. Do not begin M2–M5.
