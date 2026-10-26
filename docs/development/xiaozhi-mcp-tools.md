<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi MCP tools

The Common Tools and User Only Tools model follows
[`betamoojw/xiaozhi-esp32`](https://github.com/betamoojw/xiaozhi-esp32/tree/c435f99a88066029ec2dbd2d758c8cc9d051565b),
commit `c435f99a88066029ec2dbd2d758c8cc9d051565b`.

## Inventory

Capability registration is deliberate. `panel10` currently exposes:

| Group | Tools |
| --- | --- |
| Common | `self.get_device_status`, `self.audio_speaker.set_volume`, `self.screen.set_brightness`, `self.screen.set_theme` |
| Board | `self.knx.get_status`, `self.knx.list_objects`, `self.knx.get_object`, `self.knx.set_object`, `self.knx.read`, `self.knx.write` |
| User Only | `self.get_system_info`, `self.reboot`, `self.upgrade_firmware`, `self.screen.get_info` |

Common tools are listed before board tools for a stable cloud prompt prefix.
User Only Tools are omitted by default and serialized with
`annotations.audience = ["user"]` when requested. Other variants return no
device-tool capabilities because their Xiaozhi audio path is unsupported.

The upstream camera, snapshot upload, remote image preview and asset-download
URL tools are not registered. The local panel lacks the corresponding safe
camera/upload/preview/asset runtime contracts; listing unsupported tools would
mislead the model.

## Dispatch

`main/knx/knx_mcp.c` owns JSON-RPC dispatch, schema validation, bounded
`tools/list` pagination and the ordered inventory. It supports `initialize`,
`ping`, `tools/list` and `tools/call`. Unknown properties, duplicate
properties, wrong types and out-of-range integers are rejected.

`main/xiaozhi/xiaozhi_mcp_tools.c` adapts supported calls to existing project
services. Tool callbacks run in the Xiaozhi worker, never the WebSocket event
task. Theme changes publish a UI rebuild event, and storage operations run
outside the display lock.

The dispatcher requires the adapter symbols from that translation unit. Do not
define weak unsupported fallbacks in the dispatcher: the static-library linker
can resolve its references to those fallbacks and omit the real panel10 adapter.
Other variants use the explicit unsupported implementation in the adapter file.

The port tightens two upstream boundaries:

- a user-only call is rejected until the current initialized connection has
  requested a list with `withUserTools: true`; disconnect and `initialize`
  clear this opt-in;
- `self.upgrade_firmware.url` is required and uses the existing bounded HTTPS
  OTA path instead of accepting the upstream accidental description-as-default.

The audience annotation and list opt-in are protocol signals, not proof of a
separate local user identity. They prevent accidental/default model access but
do not replace trust in the paired Xiaozhi service.

## Checks

The `knx_mcp` host test covers default and user-only discovery, annotations,
session reset, cursor handling, argument validation, KNX permissions and
bounded responses. A `panel10` firmware build verifies the ESP adapters. Cloud
invocation of the new device tools requires the hardware acceptance below.
The `knx_mcp_link` regression links dispatcher and adapter as separate objects
in one static library, then checks discovery and dispatch through the public
MCP entry point. It reproduces missing Common Tools in the original 45ef89d
image; after the fix, the final ELF must contain strong adapter symbols.

## Waveshare trial, 2026-10-26

COM24 is a Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1, P4 v3.2 with 32 MiB flash.
The initial clean `45ef89d` build selected the weak unsupported adapters and was
not flashed. The local adapter-link fix built as `45ef89dd-dirty`, app SHA256
`e087e34fdcaedc2938d18cfaf6dd3eabf2178b56dc33912667433be0bb247548`, ELF SHA256
`b1189df9f0985665cf75275a5fade5a6cb9f84c94c2a7818a6f36864f68755eb`.

Seven host suites, 337 browser tests and the web-source checks passed. The new
archive-link test failed with the old weak fallbacks and passed with the fix;
the firmware ELF contains strong adapter symbols. The app-only stub flash and
readback passed. Metadata/NVS, coredump and complete LittleFS checksums matched
before boot. The running source stamp, boot ELF and valid OTA state matched.

Post-boot settings, layout, palettes, KNX configuration, assistant preferences
and Chinese font metadata matched the baseline. English, Classic Emoji,
180-degree rotation, volume 60, active brightness 75 and the Classic palette
were retained; the boot splash closed and all 30 KNX objects loaded.

Cloud voice calls returned the correct initial status, changed volume 60 to 50
and active brightness 75 to 55, and selected the `ocean` theme. An incorrectly
recognized `oan` theme ID was rejected; the corrected `ocean` call succeeded.
MCP call logs and independent HTTP parameter reads agreed. The owner confirmed
that the functions worked and clarified that the remaining symptom was the
previously reported intermittent full-screen white flash. No new restart,
freeze or touch problem was reported. Screen stability is not accepted: the
white flashes remain unresolved.

After testing, volume 60, active brightness 75 and `classic_v1` were restored.
Final configuration comparisons matched the baseline, the assistant returned
to `ready`, and there was no new crash report or unexpected restart through
463 seconds of uptime. This bounded trial is not an all-day stability result.
After the owner stopped operating the panel, a read-only recheck at 2,183
seconds again matched the original configuration, with the same boot sequence
and no retained crash report.
User-only discovery and denial are host-tested; cloud restart and OTA calls
have not been exercised on this image.
