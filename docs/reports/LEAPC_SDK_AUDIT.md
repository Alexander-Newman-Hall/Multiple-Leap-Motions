# Hyperion 6.2 SDK provenance, extraction and API verification

Research date: 2026-11-15 UTC. **The supported development baseline is the SDK
from the official Hyperion 6.2.0 Windows installer.** An earlier plugin-derived
probe was superseded and is not a supported build or release baseline.

This report distinguishes the tracking software installer version, client DLL
version, development-header compile, runtime load, and physical hardware results.
The SDK installed on the owner's Windows PC remains unknown until recorded there.

## Exact official source and measured versions

The [official Leap Motion Controller 2 downloads page](https://leap2.ultraleap.com/downloads/leap-motion-controller-2/)
currently advertises Hyperion **6.2.0**. Downloaded this exact official installer:

https://s3.eu-west-1.amazonaws.com/downloads.ultraleap.com/software/tracking-software/6.2.0/tracking-software-windows-6.2.0.exe

- Bytes: **629594088**.
- SHA-256: `e219438dc5031af3dc75e115f277c4e6e33475bad21fee1e90a278e77f5fadf1`.
- HTTP Last-Modified: 2025-09-22.
- Container: Inno Setup 6.1.0.
- SDK CMake package version from extracted `leapsdk-configVersion.cmake`: **6.2.0.0**.
- SDK `lib/x64/LeapC.dll` numeric file version: **6.2.0.0**.
- DLL FileVersion/ProductVersion strings: **6.2.0.0-c98d293a**.
- DLL architecture: PE32+ Windows x86-64.

The SDK was extracted **without executing the installer**, installing services,
activating a camera, or altering system configuration. Do not infer service or
hardware availability from extraction.

| Installer-relative path | Bytes | SHA-256 |
| --- | ---: | --- |
| `app/LeapSDK/include/LeapC.h` | 142142 | `27d512f953bc7dcf2d52475e5e771519b9905bc4d9a88641b1877c2897641765` |
| `app/LeapSDK/lib/x64/LeapC.lib` | 22214 | `fef3f50d8323088ce0747a82d3ece5f61178f217685b9fd9412c1eab9b112fef` |
| `app/LeapSDK/lib/x64/LeapC.dll` | 13124160 | `a1127ed0b573a9431a50d8249ac21c7440e6480014b3fbc683225d90d87145f8` |

All required loader exports, including `LeapGetVersion`, were found in this real
6.2 DLL using `objdump -p`. The import archive is real x86-64 COFF. This is actual
SDK input, with no substitute API header or emulated DLL.

No SDK files exist in the repository's original tracked branches, submodules or
LFS. Developer files remain under ignored `work/`. The versioned
`cmake/leapc-sdk-baseline.json` contains only metadata and hashes, including the
bundled licence, README, third-party notices, protobuf notice and exact
`MultiDeviceSample.c` from the same installer.

## Reproducible development retrieval

```sh
python scripts/fetch-leapc-sdk.py
# SDK root: <repository>/work/leapc-sdk
# Download/tool cache: <repository>/work/sdk-cache
python scripts/fetch-leapc-sdk.py --header-only
python scripts/fetch-leapc-sdk.py --cache-dir work/sdk-v6
```

The helper verifies the entire installer and extractor archive by size/SHA-256,
extracts only selected SDK files into temporary staging, validates **every**
selected SDK file, and then atomically replaces each destination. It never runs
the Hyperion installer. Existing correctly hashed SDK output avoids downloads or
extraction. The first retrieval requires approximately 630 MB even for
`--header-only`, because the header comes from the exact installer.

Innoextract is fetched automatically; no extra Windows workflow installation is
required. It is an extraction tool, not an SDK replacement:

| Platform | Official extractor archive | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Windows | https://constexpr.org/innoextract/files/innoextract-1.9-windows.zip | 520705 | `6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065` |
| Linux x64 | https://constexpr.org/innoextract/files/innoextract-1.9-linux.tar.xz | 890320 | `008efe5011476ccc4aae17c3e22038b5a1bc5c7aad2b9d4d869537bf3874d21f` |

Only the named tool executable is read from its verified archive; archive paths
are not extracted wholesale. The equivalent manual extraction actually used was:

```sh
innoextract --extract --silent --include app/LeapSDK \
  --output-dir work/sdk-v6/extracted \
  work/sdk-v6/tracking-software-windows-6.2.0.exe
```

The helper uses repeated exact `--include` arguments for its eight selected SDK
files, rather than extracting Python bindings or unrelated application payloads.
Installer, tool, raw SDK header/library/client binaries and extracted notices are
development inputs under ignored directories; none is a source-control artifact.

## Licence evidence from this exact package

The installer contains `app/LeapSDK/LICENSE.rft` (the extension is spelled `.rft`
in the package). SHA-256:
`f2a76cc828b9d554d4797b3b3747a2aa24c1fe71a3194c2137898d1a8a7e7bec`.
Its text is dated **March 22, 2024**. The current
[official Enterprise Tracking Licence](https://leap2.ultraleap.com/legal/enterprise-tracking-licence),
linked by the official LMC2 download page, is dated **11 April 2024**. Both supply
the following relevant grants/restrictions; the bundled agreement is retained
with local development files.

Bundled section 2(b)(i), Development Licence:

> install and use a reasonable number of copies of the SDK on computers owned or
> controlled by you for the purpose of developing and testing software
> applications that are for use in connection with the Ultraleap Software

Section 2(c)(ii), Distribution Licence:

> copy and distribute (or have copied and distributed) the Ultraleap
> Redistributables, solely as compiled with, incorporated into, or packaged with,
> your Ultraleap Enabled Application

Section 3(f):

> you may not, and may not enable others to, distribute the Non-Redistributable
> Materials

The definition of Redistributables identifies designated `.lib` code, `.dll`,
`.so`, sample code and other materials for incorporation/distribution with enabled
applications. No separate file-by-file annex expressly naming `LeapC.dll` as a
redistributable was found in the SDK licence, README or third-party notices.
The SDK README **does** specifically instruct using `LeapC.dll` and says:

> A popular option is to add a post-build step that copies it to your project's
> output directory.

That establishes intended application use/copying of the client DLL; it is not a
separate unrestricted SDK publication grant. Section 6(b) also requires applicable
affirmative end-user agreement handling if an Ultraleap installer does not
present the licence. Preserve all applicable notices. A public SDK source mirror
or SDK-only artifact is different from a compliant application package. This task
keeps the raw SDK out of Git and leaves runtime-client packaging decisions
explicit in the release documentation.

The old agreement endpoints referenced in the header returned HTTP 403; no access
controls were bypassed. The licence inside the officially downloaded installer
was extracted and read directly. The separate official Hyperion tracking service
remains necessary; a client DLL alone does not install or replace it.

## Verified API behavior in the actual 6.2 header and sample

Also checked the current official [multi-device example](https://docs.ultraleap.com/api-reference/tracking-api/examples/multi-device-example.html),
[functions reference](https://docs.ultraleap.com/api-reference/tracking-api/group/group___functions.html),
and [LeapC guide](https://docs.ultraleap.com/api-reference/tracking-api/leapc-guide.html).

- The bundled `samples/MultiDeviceSample.c` uses
  `eLeapConnectionConfig_MultiDeviceAware`, opens discovered devices, subscribes
  with `LeapSubscribeEvents`, and routes tracking through `msg.device_id`.
- `LeapOpenConnection` is nonblocking. Service establishment waits for polling;
  its return is not proof of service connectivity. Wait for the connection event.
- Device discovery uses `message.device_event->device.id`; its outer
  `message.device_id` is 0. Tracking uses the outer device ID. The real device
  status event has `device`, `last_status` and `status` members.
- Poll event pointers expire at the next poll or device/connection close. Copy
  observations before polling again. Concurrent polling returns
  `eLeapRS_ConcurrentPoll`; timeout means `eLeapEventType_None`.
- The connection-lost event may take **up to five seconds of polling** to arrive
  when no service transaction is performed after loss. Stale-frame detection and
  service notification timing are separate; do not assume a 500 ms loss event.
- `tracking_frame_id` is signed 64-bit and intended to increase, with possible
  gaps. The camera `info.frame_id` can increase faster. SDK timestamp and local
  monotonic arrival time are separate clock domains.
- Native hand `id` changes after loss/reacquisition. Visible time is microseconds;
  positions are mm and velocity mm/s. Raw palm position and SDK stabilized
  position are different; do not introduce the latter's smoothing silently.
- Confidence remains documented as unused/always 1.0. Do not advertise it as
  meaningful live confidence.
- Serial discovery supports a null buffer to obtain `serial_length`, followed by
  a sized buffer call. Handle actual results/length rather than assuming one
  first-probe return code.
- DeviceLost includes detach, service loss or other events implying no further
  frames. Historical DeviceFailure is no longer generated. Empty-hand frames are
  healthy tracking input; the SDK's message pump must be serviced efficiently.

## Checks actually run

- Official installer full download and size/SHA-256: **PASS**.
- Innoextract 1.9 extraction without installer execution: **PASS**.
- Helper end-to-end, cached verified installer/tool → all eight selected SDK
  files → atomic output under `work/leapc-sdk`: **PASS**.
- Helper cached full and header-only reruns: **PASS**.
- `python tests/test_FetchLeapcSdk.py`: **4/4 PASS** (cache, verified replacement,
  digest/short/oversize rejection preserving prior file, network failure cleanup).
- `python -m py_compile scripts/fetch-leapc-sdk.py`: **PASS**.
- C++20 syntax-only compilation using this actual 142142-byte header: **PASS**,
  with four expected SDK anonymous-struct pedantic warnings.
- Current loader's required exports against actual 6.2 DLL: **PASS**.

Adapter, native Windows compile/DLL load and release smoke results belong in the
implementation/build reports. Physical Windows two-camera tracking, USB hotplug,
service restart and TouchDesigner receipt remain **UNTESTED ON HARDWARE** until
actually performed on the target PC.

## First Windows CI fetch failure and correction

Windows Release run `43777007955`, job `133005467975`, verified both installer
and extractor downloads but failed before any Windows compilation:
`extracted Hyperion SDK file failed verification: include/LeapC.h`.
The extractor returned success in approximately 114 ms and produced no selected
files. This was a helper portability defect, not evidence of changed SDK bytes.

Upstream innoextract 1.9 source establishes the cause:
`src/setup/filename.hpp:35` uses backslash as `path_sep` on Windows and slash on
Linux; `src/cli/extract.cpp:283` compares include filters literally against those
native output paths; `src/cli/main.cpp:355` forwards the supplied filters without
normalizing them. The helper supplied slash-separated include filters on both
platforms, so Windows selected zero files and still returned exit 0.

The helper now builds Windows filters using `PureWindowsPath` and retains POSIX
filters on Linux. It still verifies every expected size/hash without relaxing
any SDK digest. Failure diagnostics now distinguish missing files from wrong
bytes and report only expected/actual size/hash and a bounded file-name/size
inventory, never SDK contents. Three regressions cover separator conversion,
missing-file diagnostics and actual size/hash diagnostics: **7/7 helper tests
PASS** in cloud. Native Windows extraction/build remains pending the corrected
CI run; the failed run did not compile or test the Windows application.
