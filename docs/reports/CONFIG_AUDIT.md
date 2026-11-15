# Configuration and alias audit — 2026-11-15

Baseline: `handbridge-v2` at `5f547181f6d512ab6a8d2146022227e26cec48d2`; source identical to `master` `054e578525a5d5202ea8f6eb417ce55ded7d61b8` apart from planning documents. This report distinguishes baseline defects from the narrow repair made during Phase 0/1. Windows paths and hardware behavior have not been exercised on Windows.

## Confirmed baseline behavior and defects

References in this section are **baseline** lines, retrievable with `git show 5f54718:<path>`.

| Evidence | Observation | Decision |
| --- | --- | --- |
| `src/core/ConfigManager.cpp:14`, `:55`, `:61` | No-argument load/save resolves `%LOCALAPPDATA%/LeapApp/config.json` through `SHGetKnownFolderPath`; the Windows-only helper prevents an SDK-independent Linux compile. | Preserve Windows location; guard Windows implementation. Non-Windows callers must supply a filename. |
| `src/core/ConfigManager.cpp:35`, `src/core/ConfigManager.h:102` | Constructor initializes gain fields only. OSC port, low latency and every send flag are indeterminate; missing-file load returns immediately. | **Repair:** explicit safe defaults. First-run output could otherwise be disabled or routed unpredictably; this is a source defect, not proof of the reported field failure. |
| `src/core/ConfigManager.cpp:83`, `:84` | Parsed config fallback is `127.0.0.1:9000`. A saved `osc_port`, including 7000, takes precedence. | Preserve loopback/9000 defaults and valid saved ports; validate range 1–65535. |
| `src/ui/UIController.cpp:15` | Existing UI constructor selects palm, wrist, all five tips, pinch and grab; optional orientation/velocity/normal/visible time/isExtended are false. | Use these source-backed flags for missing config; there was no defined ConfigManager default to preserve. |
| `src/core/DeviceAliasManager.cpp:75-79`, `src/core/ConfigManager.cpp:159` | Alias manager emits `deviceAliases`; config saver indexes `device_aliases` in that temporary, yielding null. | **Repair:** canonical file key `device_aliases` contains the flat serial→alias map. |
| `src/core/ConfigManager.cpp:93`, `src/core/DeviceAliasManager.cpp:52` | Config loader strips outer snake_case container, then alias loader expects camelCase wrapper; flat aliases clear/disappear. | **Repair:** normalize known containers explicitly. |
| `src/core/DeviceAliasManager.cpp:31`, `:68` | Existing alias loading accepts duplicate aliases and calculates next index using signed `int` plus one. | **Repair:** reject duplicate maps transactionally; use checked unsigned index. Prevents cross-camera address collisions and signed overflow. |
| `src/core/ConfigManager.cpp:141` | Saving to a bare relative filename tries to create an empty parent directory. | **Repair:** only create nonempty parent directories. |
| `src/core/ConfigManager.cpp:184` | File stream error bits are never checked, so disk write failure can return success. | **Repair:** flush/check stream state. Atomic replacement and backup are still deferred. |
| `tests/test_ConfigManager.cpp:23` | Existing test calls nonexistent `saveConfig(filename)`, so does not compile against the checked-in public interface. Alias checks also rediscover in original order, allowing a lost mapping to be reassigned unnoticed. | **Repair:** use `save(filename)` and test reversed discovery order plus exact serialized keys. |
| `configs/handbridge.example.json:2` | Example is proposed version 2, while legacy loader silently ignores nested fields. | **Repair:** reject `schema_version` until a real migration exists. Add working `configs/legacy-default.example.json`. |

The tracked JSON inventory contains no actual user `config.json`; build-generator JSON files and the proposed example cannot establish a deployed port or saved flags. There is no evidence to change the documented 9000 fallback.

## Implemented compatibility contract

Accepted legacy alias inputs:

```json
{"device_aliases":{"serial-A":"dev2","serial-B":"dev1"}}
{"deviceAliases":{"serial-A":"dev2","serial-B":"dev1"}}
{"device_aliases":{"deviceAliases":{"serial-A":"dev2","serial-B":"dev1"}}}
```

Save always emits the first shape. The alias manager's `toJson()` API retains its original camelCase wrapper. If both non-null outer containers occur, input is rejected as ambiguous. Mixed wrapper/map entries are rejected. A null snake_case container from the broken saver has no recoverable mapping; preserve existing in-memory aliases, or start empty in a new process. Do not claim lost aliases can be restored without a backup.

Duplicate aliases, non-string values, malformed `dev` indices and index exhaustion reject the candidate alias map and retain the old map. Original `dev-1` and `dev+1` strings accidentally accepted by `stoi` are now rejected as malformed. `dev0` and leading-zero decimal suffixes remain accepted. Existing normal `devN` aliases retain their exact text.

Legacy load now validates root type, unsupported versioned schema, integer port range (without float truncation/overflow), supported hand-assignment text, JSON field types, and complete candidate aliases/flags before committing; failure leaves previous settings intact. Settings absent in a legacy reload preserve prior flags, assignments and aliases, matching the previous selective-loader intent; OSC host/port/latency still use the original parse fallbacks. Explicit setter APIs remain unvalidated for legacy source compatibility; the new headless CLI validates before use.

Saved fields remain `osc_ip`, `osc_port`, `low_latency_mode`, `hand_assignments`, `device_aliases`, and `booleanSettings`. Gain parameters are not serialized by the baseline and remain outside this repair. No v2 schema, named presets, atomic backup save or public config migration command is implemented in this task.

Hand assignments preserve existing storage behavior: `setDefaultHandAssignment` erases for empty string or uppercase `NONE`; otherwise it stores text verbatim. File loading accepts empty or case-insensitive `left`, `right`, `both`, `none`, `disabled`, retains original case, and rejects typos transactionally. `getDefaultHandAssignment` returns empty when absent. This audit does not redefine legacy sorter filtering or conflate `NONE` with the v2 disabled semantic; see tracking audit and engine command documentation.

## Regression coverage and remaining work

`tests/test_ConfigManager.cpp`: safe defaults/missing file; complete flag/IP/port/hand round-trip with inverse alias discovery; all three alias containers; broken null aliases; ambiguous containers; unsupported schema; invalid/overflow/floating port and root; late-field transactional rejection; supported hand assignments and typo rejection.

`tests/test_DeviceAliasManager.cpp`: unique/stable assignment, clear/restart, inverse-discovery persistence, duplicate-map rollback, malformed-map rollback, signed-int boundary index restoration.

CLOUD verification: directly compiled the two configuration sources and both test files with C++20/GCC and CMake-extracted, pinned GoogleTest/JSON dependencies. **17/17 PASS**, zero skips (10 configuration, 7 alias). XML: ignored `work/config-tests.xml`. Command:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread -Isrc \
  -Iout/build/core-debug/_deps/nlohmann_json-src/single_include/nlohmann \
  -Iout/build/core-debug/_deps/googletest-src/googletest/include \
  -Iout/build/core-debug/_deps/googletest-src/googletest \
  src/core/ConfigManager.cpp src/core/DeviceAliasManager.cpp \
  tests/test_ConfigManager.cpp tests/test_DeviceAliasManager.cpp \
  out/build/core-debug/_deps/googletest-src/googletest/src/gtest-all.cc \
  out/build/core-debug/_deps/googletest-src/googletest/src/gtest_main.cc \
  -o work/config-tests
./work/config-tests --gtest_output=xml:work/config-tests.xml
```

One inherited warning remains: unused `alias` parameter in the `getAssignedHand` placeholder. Integrated build/test evidence is recorded in `docs/CURRENT_STATE.md` and `docs/SESSION_HANDOFF.md`. Windows default-path APIs are **UNTESTED ON WINDOWS**. Explicit-path file/alias regressions pass Windows CI; see final CURRENT_STATE. SDK hardware tracking and TouchDesigner receipt are **UNTESTED ON HARDWARE**.

Next configuration task after the reliability baseline: introduce an explicit versioned schema/migrator with atomic backup writes and presets, preserving these legacy fixtures and safe rejection behavior. This is deferred behind the project's single next recommended task in the session handoff.
