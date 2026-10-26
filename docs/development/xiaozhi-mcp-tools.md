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
invocation of the new device tools still requires hardware acceptance.
