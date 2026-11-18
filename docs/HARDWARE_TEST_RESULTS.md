# Physical hardware test results

## Apple Silicon 2.0.1, 2026-11-18

On Apple Silicon, the engine from the extracted transfer ZIP
connected to the installed Hyperion 6.2.0 service and received fresh frames from
two physical Leap Motion Controller 2 cameras for 153.3 continuous seconds.
An independent receiver on a temporary loopback port decoded nonzero palm
coordinates when the operator moved a hand over each camera.

| Camera | Frames received | Frames processed | Reconnect attempts | Nonzero palm packets received |
| --- | ---: | ---: | ---: | ---: |
| dev1 | 18,157 | 18,132 | 0 | 762 |
| dev2 | 18,107 | 18,086 | 0 | 24 |

Both devices continued delivering frames with hands out of view. The packaged
controller showed two streaming cameras and exited while the same engine kept
running. The engine exited cleanly with zero SDK poll errors and zero OSC send
errors. The receiver recorded zero decode errors. This run used an isolated engine endpoint, fresh settings and a loopback receiver.

Physical unplug/replug, a service restart, a show-length soak and receipt
inside the installation's TouchDesigner project remain open on 2.0.1.
The archive and automated fault-injection checks are in the
[Mac validation report](reports/MACOS_ARM64.md).

## Windows hardware status

The Windows physical test remains pending. Cloud checks include the genuine-header
adapter with injected SDK calls. Windows DLL and deployment checks are recorded
in CURRENT_STATE.

For every run, complete the checklist in `docs/HARDWARE_TEST_PLAN.md` and append the following record. Do not label simulated/cloud tests as physical.

## First Windows test report — pending

- Date/time (UTC or local with offset): TBD
- Tester: TBD
- Repository commit SHA: TBD
- Binary/hash: TBD
- Windows version: TBD
- Hyperion service / LeapC versions: TBD
- Cameras: TBD (models/count; redact exact serials when sharing)
- USB ports/controller/hub: TBD
- TouchDesigner version: TBD
- OSC destination/port: TBD
- Active profile/schema: TBD
- Tested with real hand motion: NOT RUN
- Two simultaneous cameras: NOT RUN
- Unplug/reconnect: NOT RUN
- Tracking service restart: NOT RUN
- TouchDesigner OSC In CHOP/DAT verified reception: NOT RUN
- Soak test duration/result: NOT RUN
- Error logs/metrics / artifact links: TBD
- Open issues: TBD
