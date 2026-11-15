# Windows x64 Release portable preview

This is a developer preview, not a hardware-accepted release. The Release workflow
builds the real SDK-header adapter and packages both native applications, Qt/QML
plugins, app-local Microsoft Release runtime, notices and corresponding Qt source.
Ultraleap SDK binaries, its tracking service, debug libraries and build tools are
excluded. Current actual results are in [CURRENT_STATE](CURRENT_STATE.md).

## Download and run

Open the [Windows Release workflow](https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/actions/workflows/windows-portable.yml)
on the development branch and choose a **successful** run. Its
`HandBridge-2.0.0-windows-x64-preview` artifact contains the ZIP and SHA-256 file.
CI uploads a package only after the extracted-package smoke succeeds. GitHub may
require sign-in to download artifacts; artifacts expire after thirty days.

Extract the artifact, then extract the inner ZIP completely. Keep both executables,
DLLs, plugins/, qml/ and qt.conf together. Qt development files, Visual Studio,
Python and a developer PATH are not runtime requirements.

```powershell
.\handbridge-control.exe --demo
```

For real input, install the official compatible Ultraleap Hyperion runtime/SDK,
then follow [LEAPC_SETUP](LEAPC_SETUP.md). The **Start engine** button checks its
installed DLL before launching; **Start simulator** is separate. Closing the GUI
leaves the detached engine running. Use Task Manager to stop a detached engine,
or foreground Ctrl-C when running it explicitly.

The preview is unsigned, has no installer or automatic startup, and needs no
administrator privileges of its own. Hyperion installation/first camera
activation has separate vendor requirements. No physical camera success is
implied by a passing deployment check.

## Reproduce the package

Build prerequisites: Windows x64, licensed VS2022 C++ tools/redist files,
PowerShell7+, CMake3.24+, Python3.9+ for tests, Git and official Qt6.8.3 MSVC2022 x64.
PowerShell7 is required only for packaging; ordinary Debug builds do not add that
requirement. The package script intentionally rejects Debug configuration.

```powershell
$env:Path = "C:\Qt\6.8.3\msvc2022_64\bin;$env:Path"
python scripts/fetch-leapc-sdk.py
cmake --preset windows-release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
  -DHANDBRIDGE_WITH_LEAPC=ON -DLEAP_SDK_ROOT="$PWD/work/leapc-sdk"
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
.\out\build\windows-release\Release\handbridge-engine.exe --check-leapc `
  --leapc-library "$PWD/work/leapc-sdk/lib/x64/LeapC.dll"
cmake --build --preset windows-release --target handbridge-portable
pwsh -File scripts/Test-WindowsPortable.ps1 `
  -Archive ./out/build/windows-release/portable/HandBridge-2.0.0-windows-x64-preview.zip `
  -OutputDirectory ./work/portable-smoke `
  -ExternalLeapCLibrary "$PWD/work/leapc-sdk/lib/x64/LeapC.dll" `
  -ExpectedLeapCVersion '6.2.0.0'
```

Prefer `LEAP_SDK_ROOT` pointing to the exact installed SDK when available. The
fetch helper supplies the SDK from the official Hyperion6.2.0 installer, including
the matching6.2.0.0 client, rather than assuming the owner's installed version.
Client and service must each report6.2.0 or newer6.x. Do not commit or publish
SDK-only files.

The ZIP is under `out/build/windows-release/portable/`. Its manifest records source
revision/dirty state, Qt version, adapter compilation, external-runtime policy,
file hashes and hardware-unverified status. Reconfigure after changing Git HEAD;
the package script rejects an outdated configured revision.

## What the extracted-package test establishes

The smoke extracts the actual ZIP to a fresh directory and launches its native
processes with only Windows system locations on PATH. It clears Qt/QML/CMake/SDK
developer search settings and uses the actual Windows platform plugin. It verifies:

- Qt and Microsoft runtime modules load from the extracted package; the engine
  loads no QtGui/Quick/QML libraries.
- The real external LeapC development DLL loads with its expected version and
  required exports; missing/invalid runtime diagnostics remain actionable.
- A bounded real-client run opens/polls/cleans up the actual SDK with no installed
  tracking service or cameras. It reports the real provider and recovery errors,
  never invents devices/frames or sends a synthetic pose.
- Real UDP packets continue through GUI normal close, kill and reopen.
- Overview/processing render real native PNG captures; logs and manifest checks
  are retained as workflow evidence.
- The ZIP contains no LeapC.h/.lib/.dll, PDBs or debug Qt runtime.

This removes developer search paths on a Windows CI machine. It is **not** a
fresh consumer Windows10/11 installation, physical GPU-driver acceptance, a
service connection test, camera validation or proof of TouchDesigner receipt.
Follow the [hardware plan](HARDWARE_TEST_PLAN.md) on the target PC next.

## Dependencies and notices

`windeployqt` handles Qt DLLs/QML/plugins; the script copies Microsoft release CRT
only from the documented VS redistributable directory, never Debug_NonRedist.
Qt's optional Mesa/FFmpeg blobs are not bundled. The ZIP includes LGPL/GPL/Qt
exception texts, preserved dependency notices and exact Qt6.8.3 qtbase /
qtdeclarative corresponding-source archives. Dynamic replacement of compatible
Qt libraries is unrestricted by this application; manifest hashes are diagnostic.
See `packaging/THIRD_PARTY_NOTICES.txt` and [DEPENDENCIES](DEPENDENCIES.md).
