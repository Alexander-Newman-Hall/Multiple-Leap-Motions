Hand Bridge 2.0 — Windows 10/11 x64 developer preview

Extract the complete ZIP to a normal writable folder. Do not run the executables
from inside the ZIP. Keep plugins/, qml/, Qt DLLs and the two executables together.
Qt and the Visual C++ release runtime are app-local; Python, Qt development tools,
Visual Studio, a development PATH and administrator privileges are not required.

SIMULATION (no camera or Ultraleap installation required):
  handbridge-control.exe --demo
Or run a foreground engine, then the controller:
  handbridge-engine.exe --simulate --headless
  handbridge-control.exe
Closing the control window leaves the engine and OSC running. End an explicitly
started foreground engine with Ctrl+C. The detached demo engine can be ended via
Task Manager. OSC defaults to 127.0.0.1:9000, legacy scalar addresses.

REAL CAMERAS (requires a separately installed compatible Ultraleap runtime/SDK):
  handbridge-engine.exe --check-leapc --leapc-library "C:\absolute\path\LeapC.dll"
  handbridge-engine.exe --headless --leapc-library "C:\absolute\path\LeapC.dll"
  handbridge-control.exe
Use the actual installed x64 runtime path. The SDK-backed build is recorded in
package-manifest.json. LeapC.dll and the Hyperion tracking service are external
prerequisites, not part of this ZIP. The probe verifies loading/API exports and
version only; it does not prove that any camera/service works.

Profiles/state default to the current user's local application-data directory.
Use engine --help for --config, --port, --endpoint and --state-dir. Startup logs
and the GUI identify the selected source; simulation never proves hardware.
See THIRD_PARTY_NOTICES.txt and corresponding-source/ for dependency licenses and
Qt source. Unsigned developer preview: no installer, Windows Service, auto-start,
camera driver or elevated system changes. UNTESTED ON PHYSICAL WINDOWS HARDWARE.
