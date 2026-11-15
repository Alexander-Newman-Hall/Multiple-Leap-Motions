# Official references and repository evidence

These links are documentation starting points. Codex must verify installed SDK versions and compare public examples against exact headers and current repository source rather than guessing APIs.

## Product and API

- Original repository: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions
- Reference `master` commit: https://github.com/Alexander-Newman-Hall/Multiple-Leap-Motions/commit/054e578525a5d5202ea8f6eb417ce55ded7d61b8
- Ultraleap Hyperion: https://docs.ultraleap.com/hand-tracking/Hyperion/index.html
- LeapC Guide: https://docs.ultraleap.com/api-reference/tracking-api/leapc-guide.html
- LeapC multi-device example: https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html
- LeapC functions (`LeapSubscribeEvents`, open/close): https://docs.ultraleap.com/api-reference/tracking-api/group/group___functions.html

## Native UI / IPC

- Qt 6 Qt Quick: https://doc.qt.io/qt-6/qtquick-index.html
- Qt QLocalServer: https://doc.qt.io/qt-6/qlocalserver.html
- Qt QLocalSocket: https://doc.qt.io/qt-6/qlocalsocket.html

## Cloud and CI

- Codex Cloud overview: https://help.openai.com/en/articles/20001545-using-codex-cloud
- `AGENTS.md` instructions: https://learn.chatgpt.com/docs/agent-configuration/agents-md
- GitHub Actions: https://docs.github.com/en/actions

## TouchDesigner

- TouchDesigner official documentation: https://docs.derivative.ca/
- OSC In CHOP: https://docs.derivative.ca/OSC_In_CHOP
- OSC In DAT: https://docs.derivative.ca/OSC_In_DAT

## Snapshot caveat

This specification package is a design/review artifact. Individual source-code findings were read statically. All hardware behavior and build status remain unverified until a tested Windows commit records evidence.

## 2026-11-15 verification

Official Hyperion, LeapC guide/functions and multi-device pages fetched over HTTPS
and inspected; exact URLs, hashes and observed API constraints are in
[TRACKING_AUDIT](reports/TRACKING_AUDIT.md). No installed LeapC SDK headers are
available in cloud. This verifies public documentation only, not a particular
installed Hyperion version or hardware/API compilation. Native UDP error behavior
was checked against the pinned oscpack sources and kernel error tests; see OSC_AUDIT.
