# Current state — native controller rebuild in progress

Branch: `codex/handbridge-native-controller`; base:
`30cd5e359ea410b1ad07ffc91e970f2d95bfb522` on the preserved
`codex/handbridge-phase0-reliability` branch (draft PR1). Original master and
handbridge-v2 remain unchanged.

The user explicitly resumed cloud work to rebuild the GUI: a modern VST-style
native controller with controls that affect real engine settings. This supersedes
the previous pause/order restriction. Phase 2 hardware adapter is still blocked;
Phase 3 plus the necessary settings/processing slice is authorized against mocks.

Baseline evidence: Linux Debug/Release/ASan+UBSan40/40, Windows SDK-free build and
39 passes+one POSIX-only skip. This does not validate new controller work yet.

Active changes: typed/validated schema2 settings and processing; local same-user
IPC and independent headless host; Qt Quick/QML controller with five pages,
revisioned apply/save status, profiles/diagnostics; native builds and integration
checks. No GUI build or physical result is claimed before the actual run.

Hardware/Hyperion/USB/TouchDesigner: UNTESTED ON HARDWARE. New engine source is
still simulation-only. Original ImGui source is retained for rollback.

Next action in this task: integrate/build the native GUI and test parameter changes
against actual UDP, then verify that closing/killing/reopening the controller does
not stop the engine. Final handoff replaces this checkpoint with exact results.


Portable settings/processing checkpoint: strictschema2andlegacyimport, real OSC
mute/rate/channels/alias/handrouting, timebasedfilters/transformunits, optional
gesturehysteresis and raw/processedpreview implemented. Final SDK-free Debug
CTest55/55PASS,0skips,3.76s (work/controller-core-results.xml). Legacygoldens
unchanged. GUI/IPC/native tests are under final integration separately.


IPC checkpoint: QtCore/Networkhost +independentstd::jthreadworker, bounded sameuser
protocol/CAS, atomicprofilesandstartupbackups, measuredtelemetry and diagnostics
implemented. Actualnative integrationworktree58CTestentriespassed before final
persistence/layout regressions; final results will supersede thischeckpoint.
Engine ldd verifiesQtCore/Networkonly. GUI sources commit separately next.
