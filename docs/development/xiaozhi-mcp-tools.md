<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi MCP tools

The Common Tools and User Only Tools model follows
[`betamoojw/xiaozhi-esp32`](https://github.com/betamoojw/xiaozhi-esp32/tree/c435f99a88066029ec2dbd2d758c8cc9d051565b),
commit `c435f99a88066029ec2dbd2d758c8cc9d051565b`.

## Inventory

Capability registration is deliberate. `panel10` currently exposes:

| Group | Tools |
| --- | --- |
| Common | `self.get_device_status`, `self.audio_speaker.set_volume`, `self.screen.set_brightness`, `self.screen.set_theme`, `self.get_network_info` |
| Home Assistant (Common) | `self.ha.list_entities`, `self.ha.get_entity`, `self.ha.control_entity` |
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

## Home Assistant queries

`self.ha.list_entities` discovers only entity bindings in the saved panel layout:
widgets and their secondary bindings, house rooms, explicit energy sources and
alerts. It shares `ha_layout_entities_collect()` with HA subscriptions. Entities
seen only in editor searches, settings-only top-bar/saver bindings and implicit
HA Energy statistics are not exposed. `self.ha.get_entity` rechecks this allowlist
on every call, so removing an entity from the layout revokes access immediately.
Queries expose no token or HA URL, and send no HA request or config write.

Discovery supports optional `search` (ID or HA friendly name), exact `domain`,
`limit` (1–5) and `offset`. Results sort by ID and include `next_offset`, `has_more`
and `layout_signature`. Continuations must repeat the signature and filters;
a changed entity set returns an error requiring discovery from offset zero.
An exhausted collector fails closed instead of returning a silently partial scope.
The default maximum page is five entities; serialized data stays within 2,800
bytes before MCP string escaping. Large ID/state scratch buffers use PSRAM.

Each query reads the current shared HA model. Existing WebSocket subscriptions
update values and names, including attribute-only updates. Removed HA entities
become `unavailable` in both the compact stream and fallback event paths.
`connected`, `sync_complete`, `state_revision` and `consistent` describe the
sample; retry a non-consistent sample. Per-entity `known`, `valid` and `stale`
distinguish missing, unknown/unavailable and disconnected/unsynchronized data.
Cached values are not a fresh HA round trip or evidence of physical actuation.
Detail timestamps distinguish panel receipt uptime from HA Unix change time.

Details contain selected scalar operational attributes, with
`attributes_complete: false`; the panel cache is already compacted. Tokens,
camera/media URLs and arbitrary integration attributes are not forwarded.
The tool inventory stays fixed: state changes update query results, not
`tools/list`. No unsolicited cloud push or resource subscription is advertised;
the assistant must call the tools again when asked for current values.

Host tests link the production layout collector, model and MCP reader. They cover
scope changes, renamed entities, state/attribute updates, paging, invalid cursors,
unknown/deleted entities, disconnect/reconnect synchronization, data filtering
and allocation failures. The COM24 trial below covers the HA tools separately
from the earlier network-tool acceptance.

On 2026-10-26 all nine MCP/KNX suites passed; added boundary cases also passed
for escaped responses, collector saturation and complete 17-tool discovery.
The strict documentation build and source-size gates passed. ESP-IDF 6.1 built
`panel10` as `1bea71a8-dirty`, ELF `46c0f2ec`; the image is 6,737,280 bytes,
leaving 78,464 bytes in the existing OTA partition. Strong HA, network and device
adapter symbols are present. The subsequent incremental build was byte-identical.

The authorized COM24 app-only update used the supported P4 stub. Application
readback matched; metadata/NVS, crash storage and complete LittleFS digests were
unchanged before boot. Running ELF, source stamp and valid OTA matched. Saved
English, Classic Emoji, 180-degree rotation, voice wake, layout, themes, KNX
configuration and the installed Chinese font matched the pre-flash snapshots.

Real cloud discovery and eight tool calls returned successfully. The assistant
reported the nine distinct layout entities and WLED-MTech's initial `off` state;
missing brightness was reported as unknown. After a manual change, the captured
reply reported `on` and brightness 137. The owner confirmed fresh on/off readings,
restored the light to off and confirmed normal replies, auto-close, touch and
physical output. At 403 seconds uptime, 83 samples over 340 seconds had no HTTP
failure, unexpected restart or retained crash. The Xiaozhi task's observed stack
margin was 7,140 bytes. Layout add/remove access was host-tested, not edited on
the device. Existing transient LVGL-lock and already-disabled I2S messages remain
in the logs; they also occurred before this update. This bounded trial does not
resolve the previously reported intermittent white flashes or prove all-day stability.

## Home Assistant control

`self.ha.control_entity` takes an exact layout `entity_id`, `action` and optional
string `value`. Discovery includes `controllable`; `get_entity.controls` gives
the available actions, numeric limits/steps and cached choices. The model must
discover the target and inspect its controls first. Options are bounded samples;
`options_complete: false` means omitted choices, not permission to invent one.

Supported actions cover lights (power, percent brightness, RGB hex, Kelvin,
effect), switches/input booleans, fans (power, percent speed, oscillation,
preset), covers (open/close/stop/position), climate (single target temperature,
HVAC/fan/swing/preset mode), numbers, selects, buttons, scenes and scripts.
Capability flags follow HA's [fan](https://contributors.home-assistant.io/core/entities/fan/)
and [climate](https://contributors.home-assistant.io/core/entities/climate/) contracts.
Climate compaction retains feature flags, temperature step and presets.
Sensors and unsupported domains/actions remain query-only; no arbitrary service,
toggle, wildcard, area, device or multiple-entity target is accepted.

Every command rechecks the current saved layout, connected/synchronized HA state,
availability and capabilities. Values must match the cached range/step or exact
option; percent values are integers 0–100, RGB is `#RRGGBB`, and temperatures use
HA's units. Removing a layout binding revokes both read and control access.
Existing HA authentication and permissions still apply. No new connection or
credential is introduced, and configuration is never changed by these tools.

The worker builds escaped JSON and calls the existing HA WebSocket service path
once. `submitted: true` means transport submission, never confirmed execution;
`state_confirmed` remains false. Query again for HA feedback before claiming
completion. There is no optimistic state write, automatic retry or synchronous
wait while holding a model/display lock. The shared request builder now rejects
invalid/OOM service data instead of silently sending a command without its target.
Service sends also bypass the generic transport retry so ambiguous failures
cannot duplicate a scene, script or button action; query retries are unchanged.

Host coverage exercises the control matrix, capabilities, ranges, exact choices,
JSON escaping, offline/synchronization and layout revocation, transport failure,
feedback and allocation failures through the production request builder.
The earlier COM24 trial above verified queries; the separate control trial follows.

Local validation on 2026-10-26 passed all nine MCP/KNX suites, followed by both
affected MCP suites after capability/boundary refinements. The separate HA
control/attribute suite passed 49 checks using the Windows time compatibility
shim; strict documentation and source-size checks passed. No real HA command
or device configuration write was made during these tests.
ESP-IDF 6.1 built `panel10` as `1bea71a8-dirty`, ELF `0f367eef`, with strong
HA control/adapter symbols. The app is 6,743,792 bytes, leaving 71,952 bytes
in the unchanged OTA partition. The requested repeat build was byte-identical.

The authorized COM24 app-only stub flash passed application verification and
unchanged metadata/NVS, coredump and complete LittleFS checksums before boot.
Running ELF and valid OTA matched. Saved settings, layout, themes, KNX config
and Chinese font metadata matched afterward, including English, Classic Emoji
and 180-degree rotation. All prior local backups were retained.

Voice control switched `light.wled_mtech` from on to off and back to on. HA
service traces recorded `light.turn_off` and `light.turn_on`, each followed by
state feedback after 128 ms; KNX's transmitted count remained zero. Final blue
RGB (0, 0, 255) and brightness 137/255 matched the initial state. A misrecognized
name prompted clarification before execution. The owner confirmed physical
switching, retained color/brightness, replies, idle close and stable output.
The assistant returned to ready about 32 seconds after the final listening phase.

Thirty-eight samples from 80 to 243 seconds uptime showed no HTTP errors,
unexpected restarts or retained crashes; final checks at 245 seconds matched.
Twelve MCP calls returned successfully with no logged service-send errors.
This trial verifies WLED power control, not every supported domain/action or
all-day stability. Occasional full-screen white flashes remain unresolved,
including reports while idle with Live Display closed. Their root cause is
unconfirmed; this MCP update does not claim to fix the display symptom.

## Network query

`self.get_network_info` takes no arguments and needs no User Only opt-in.
`xiaozhi_mcp_network.c` reads the current station through `wifi_mgr` and
ESP-NETIF, returning `connected`, `setup_ap_active`, `ssid`, `bssid`,
`rssi_dbm`, `channel`, `ip_address`, `netmask`, `gateway`, `dns_servers`,
`hostname` and `mac_address`, with `interface: "wifi"`.

Addresses and the main/backup DNS list are IPv4. Unknown scalar fields are
`null`; an empty DNS list means no IPv4 servers could be read. A disconnected
station omits its old lease/AP details, even if disconnection happens during
collection. MAC and hostname may remain available offline. An AP-info timeout
does not discard readable local IP information. `connected` is the Wi-Fi
manager's connection state, not a claim that the internet or cloud is reachable.

The call runs in the existing worker, uses the serialized Wi-Fi AP-info getter
and never scans, reconnects, saves settings or reads credentials. Its fixed
output fields contain no Wi-Fi password, setup password or service token.
Allocation failure returns an MCP tool error rather than a partial snapshot.

## Checks

The `knx_mcp` host test covers default and user-only discovery, annotations,
session reset, cursor handling, argument validation, KNX permissions and
bounded responses. A `panel10` firmware build verifies the ESP adapters. Cloud
invocation of the new device tools requires the hardware acceptance below.
The `knx_mcp_link` regression links dispatcher and adapter as separate objects
in one static library, then checks discovery and dispatch through the public
MCP entry point. It reproduces missing Common Tools in the original 45ef89d
image; after the fix, the final ELF must contain strong adapter symbols.

`xiaozhi_mcp_network` tests the production collector with interface stubs:
connected/offline/setup states, disconnect during collection, absent interface,
failed reads, zero lease, IPv4 DNS filtering, JSON escaping and allocation
failure. `knx_mcp` checks discovery, dispatch without user-only opt-in,
unexpected-argument rejection and unsupported-board capability filtering.
COM24 voice acceptance of the network query is recorded below; earlier
device-tool trials cover the other Common Tools.

Local validation on 2026-10-26 passed all eight MCP/KNX suites and the strict
documentation build. ESP-IDF 6.1 built `panel10` as `1bea71a8-dirty`, ELF
`1f55bd26`, with the real network collector and device adapter linked. The app
is 6,731,776 bytes, leaving 83,968 bytes in its existing OTA partition. This
image subsequently passed the app-only COM24 trial below.

### Network voice trial, 2026-10-26

The P4 flasher stub verified the new app and unchanged metadata/NVS, coredump
and full LittleFS checksums before boot. Running source, boot ELF and valid OTA
matched the build. The splash closed after transient display-lock retries.
Cloud discovery and a tool-call response were logged successfully. The owner
asked for the panel's network information; the reply returned the correct LAN
IP and SSID, plus -75 dBm signal strength. Captured speech/replies matched the
independent settings snapshot and boot IP. The owner confirmed correct network
information, normal replies/auto-close and stable physical output.

All final settings/layout/theme/KNX/font comparisons matched. English, Classic
Emoji, the current 90-degree rotation and the owner's NTP were preserved, along
with all local backups. Thirty-eight samples through 190 seconds of uptime
showed no HTTP polling failure, unexpected restart or new crash report; the
assistant returned to ready. Gateway, DNS, subnet and MAC fields have host/build
coverage but were not individually confirmed by voice in this trial. Earlier
intermittent full-screen white flashes remain unresolved; this bounded trial
does not establish all-day stability.

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
