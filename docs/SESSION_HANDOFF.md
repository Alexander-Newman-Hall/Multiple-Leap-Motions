# Session handoff — controller rebuild checkpoint

Branch: codex/handbridge-native-controller.
Base:30cd5e359ea410b1ad07ffc91e970f2d95bfb522.
Prior completed handoff archived at reports/PHASE1_HANDOFF.md.

The latest user instruction authorizes rebuilding the GUI now, with modern VST
styling and settings controlling the engine. It supersedes the local-hardware
pause for GUI work only. Real SDK/camera verification remains blocked.

Parallel implementation owners: settings/processing (Qt-free runtime), QtCore/
Network engine IPC/worker/profile storage, C++ controller client, QtQuick views,
build/tooling and native process/UDP tests. Files live under src/runtime,
src/engine, src/ipc, src/control and tests; original ImGui sources stay intact.
Root owns integration/design review/docs. No implementation commit or new GUI
build/test PASS has been claimed at this checkpoint.

Completion must record all tested settings, UI lifecycle/UDP evidence, screenshots,
exact Windows/cloud commands/results and limitations. Keep one next recommended
task focused on installed-SDK hardware integration after the GUI is reviewable.
