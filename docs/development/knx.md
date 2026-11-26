<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# KNX

The shared main/knx service owns one UDP Routing handle and a bounded eight-entry
command queue. A low-priority task uses the existing WIFI_STA_DEF netif, follows
disconnects/IP changes, and retries failed starts after ten seconds.
It does not create Wi-Fi, DHCP or another event loop.
The component receive task only validates/updates the cache under the registry lock.
Stopping the transport never holds that lock.

Disabled service opens no socket and sends no telegrams. Existing 32-socket
configs retain spare capacity after the 12 HTTP sessions and prior reservations;
legacy builds with fewer than 24 sockets refuse enablement.

## Persistence

NVS namespace knx, key enabled, stores the enable preference.
The registry is /littlefs/interfaces/knxConfig.json.
Saving uses the shared flash gate and atomic file helper; publish the registry
only after rename succeeds. Missing files use an empty in-memory registry.
Unreadable or invalid files are never overwritten or formatted at boot.

Configuration and enable changes increment the configuration generation. Before
sending, the worker checks the active configuration, routing epoch and current
object permissions. A queued request expires when its connection ends.
Incoming self-address telegrams are ignored; no optimistic state confirmation.
Caches live only in RAM with monotonic last_update_ms timestamps.

## Widget bindings

A KNX widget stores stable object IDs in `binding.roles`. For example, a Slider saves
`{"source":"knx","roles":{"brightness_command":"A","brightness_feedback":"B"}}` inside `binding`.
Light additionally supports `switch_command` and `switch_feedback`. Sensor uses `value_feedback`,
Gauge and Graph use `number_feedback`, Binary and Contact use `switch_feedback`, and Scene uses
`action` with a string in `binding.value`. The objects API advertises `widget_roles` using the
same DPT and permission predicate as the worker. Its `widget_types` field retains the single-object
contract. Layout validation checks role names, shape and ID bounds independently of the registry,
so a layout can be restored first. Legacy `binding.object_id` and HA `entity_id` remain valid.

Cover, Climate, Fan and Select reuse the native HA widgets with role-based KNX adapters. Their
bindings require `roles`; a single object does not unambiguously describe these devices.
Cover accepts `move_command`, `stop_command`, `position_command` and `position_feedback`.
Its optional boolean `position_open_at_100` defaults to false and only belongs to Cover.
Climate uses `target_temperature_command`, `target_temperature_feedback`,
`current_temperature_feedback`, `hvac_mode_command` and `hvac_mode_feedback`.
Fan adds `fan_speed_command` and `fan_speed_feedback` alongside the existing switch roles.
Select uses `preset_command` and `preset_feedback` for DPT 20.102. Slider accepts the
existing DPT 5.001 brightness roles or the DPT 5.003 `angle_command`/`angle_feedback` pair.
Mixing the two quantities in one slider is rejected. A feedback role may be omitted, null
or an empty string. At least one object must still be assigned.
The [user guide](../knx.md#cover) lists the exact subtype and command conventions.

`knx_widget_model.cc` projects each role into the existing HA-shaped view. An assigned
feedback ID selects only live, readable bus data, even if the ID no longer resolves.
With no feedback ID, paired roles can display the last command. The worker publishes it
after the transport accepts the write. Queue admission, failed sends and loopback leave
both sources intact. External group writes update matching writable aliases. Group
responses update readable feedback only. The objects API exposes `last_command`
separately from `value` and `live_feedback`. An absent field means unknown.
Queued commands carry the routing epoch. Disconnecting or replacing the registry
invalidates both caches and pending work. The thermostat keeps its editing draft separate.

All control IDs are owned by the widget's PSRAM-preferred context and freed with it.
The four additional role IDs cost 196 bytes per interactive KNX context. Ordinary HA
contexts do not allocate this role table. Each registry object carries an eight-byte
scalar and revision pair. Cached commands use RAM and never write layout or graph storage.
Composite views track every bound object's revision, including aliases on a shared address;
registry replacement also refreshes capabilities when IDs are removed or retyped.
An external DPT 9 invalid marker (`0x7FFF`) or a complete DPT 20.102/20.105
report containing a reserved code clears the reading on every readable alias.
A truncated telegram preserves the last valid observation. Neither changes the
last valid command.

The mapping follows the supplied [KNX DPT reference, v02.02.01](https://support.knx.org/hc/en-us/article_attachments/15392631105682),
especially pages 26–32, 39 and 115–117, and the [HA KNX cover convention](https://www.home-assistant.io/integrations/knx/#cover).
The reference defines DPT semantics; configured actuator capabilities and ETS parameters
still determine which optional objects are appropriate for a particular installation.

The routing callback decodes an external write or response once per group address. Under the
registry lock it publishes the value, receive timestamp and revision to every readable alias.
The UI task checks the revision on its normal loop (20 ms delay between turns), takes the display
lock and applies up to four changed widget views per turn. A busy registry or display lock defers
the work. Each widget has its own cursor, including widgets on other pages. A telegram arriving
during a scan triggers another pass. These timings describe scheduling. Actual bus-to-LCD latency
also includes router transit, scheduling, rendering and scanout.

## API

Every route uses the existing authentication, origin/content-type and rate guard.

| Route | Operation |
| --- | --- |
| GET /api/knx | Enable preference, Routing state, counters and last error |
| PUT /api/knx | JSON containing only boolean enabled |
| GET /api/knx/config | Saved knxConfig.json |
| PUT /api/knx/config | Complete replacement registry, up to 65,535 bytes |
| GET /api/knx/objects?offset=0 | 20 cached objects per page |
| POST /api/knx/command | Exact object_id, action read/write, string value for writes |

Object pages carry the configuration `generation`, `routing_epoch` and routing state.
The editor restarts a paged read once if any of these changes. Within one connection
it uses the latest readable and writable alias observations separately. The picker
and periodic preview share the newest completed request.

Commands return HTTP 202 with queued=true, completion=asynchronous and
bus_confirmed=false. Queue/send rejection counts and transport errors appear in
status. No raw APDU or arbitrary-address endpoint is exposed.
Normal editor backups add optional knx.configuration and knx.enabled.

## AI Assistant MCP {#xiaozhi-mcp}

The existing WebSocket hello advertises MCP. JSON-RPC initialize, ping,
tools/list and tools/call run on the assistant worker; commands enqueue without
waiting for bus feedback. Metadata comes from the current registry.
Tools under self.knx are get_status, list_objects, get_object, set_object, read
and write. List responses are limited to five objects for bounded audio frames.
No configuration-import or enable tool is exposed to the AI.

## Checks

After fetching the dependency:

~~~sh
cmake -S tests/knx -B .local/knx-host
cmake --build .local/knx-host
ctest --test-dir .local/knx-host --output-on-failure
node --test tests/webui/knx.test.mjs
python tools/check_web.py
scripts/build.sh panel10 build
~~~

Override ESP_KNX_IP_DIR and CJSON_DIR for standalone host checkouts.
Coverage includes wire packets, subtype/enum boundaries, JSON fields and limits,
MCP argument/errors, import/edit/save/reload and permission-aware controls.
The composite-widget cases also exercise command polarity, percentage inversion,
temperature invalidation, reconnects, partial bindings and feedback independence.
`tests/host/test_layout_compat.c` covers complete layout validation and serialization;
the SDL controls suite exercises real Slider, Select, Cover, Climate and Fan event handlers with
synthetic KNX adapters while Home Assistant is offline. Editor tests cover role changes,
save/reload, all seven translations and real-cache previews without sample state.
Real Routing reception, reconnect, voice-to-actuator operation and concurrent
audio/display stability require a separate authorized device trial.

## Command and feedback validation, 2026-11-25

Local Windows tests used the `dev` workspace based on `e24c81a0`, host C/C++, Node
and the Panel10 SDL simulator with LVGL 9.6. The editor created a command-only
slider, saved and reloaded it, then assigned a separate feedback object to the same
slider and saved again. The actual KNX worker processed both layouts against a
simulated routing transport. Its registry snapshots drove the editor, and its native
projections drove two LVGL sliders. All 16 labels matched across eight observations.

| Observation | Command only | Independent feedback |
| --- | --- | --- |
| 80% queued, send still pending | Unknown | Unknown |
| 80% sent, then 0% sent | 80%, then 0% | Unknown |
| Next send fails or a local echo arrives | 0% | Unknown |
| Send 80%, actuator returns raw 158 on its status address | 80% | 62% |
| External command writes 20%, then the queue is full | 20% | 62% |
| Registry is replaced | Unknown | Unknown |

The KNX CTest suite passed 12/12, the full editor suite passed 685/685, and the
Panel10 LVGL run with this trace passed 292 KNX assertions. The complete C layout
validator also passed its save/reload and legacy-binding cases. To repeat the joint
test, set `BETTA_KNX_SIMULATOR` to the built SDL executable when running CTest.
For a standalone Node run also set `BETTA_KNX_SERVICE_TEST` to `knx_service_test`.

ESP-IDF 6.1.1 built the complete `panel10` v0.9.2 app, including the editor and
the verified ESP32-C6 3.1.0 image. The panel build stamp is 23:36:56 and the IDF
application descriptor records 23:22:53. The 6,812,512-byte image leaves
3,232 bytes in the 6,815,744-byte OTA slot, using 5,168 bytes of the previous
8,400-byte reserve. The application SHA-256 is
`dfd7244610e350a5d47a4f6f3386913ea7f9b65b99280aec8f7be7ce7d63f4ca`.
Physical actuator feedback, multicast loopback timing and recovery after a real
bus or router outage remain to be verified. This run used local fixtures throughout.

The [Panel10 KNX feedback validation](panel10-knx-feedback-validation.md) records
the follow-up on COM24 with an isolated simulated actuator.

## Local validation, 2026-10-25

Source: dev at f7084ce3 plus the uncommitted KNX implementation. No panel was
flashed and no telegram was sent to a real KNX network in this trial.

- Six host suites passed: codecs, configuration, MCP, wire protocol, service
  lifecycle and guarded HTTP routes. These exercise permission changes, stale
  queued commands, disconnect/reconnect, startup failure, nested JSON rejection
  and bounded MCP pagination.
- Browser tests passed (337 tests); WebUI checks and the strict documentation
  build passed. The seven catalogs include KNX controls and status text.
- The synthetic local editor passed edit/save, enable/disable, retained
  configuration, read-only controls and unknown cached values at 1280 px and
  390 px. Captures remain local. These checks do not establish hardware behavior.
- ESP-IDF 6.1 panel10 build passed with the existing partition table: application
  6,724,224 bytes, 91,520 bytes remaining in the smallest OTA slot. Other board
  manifests include the shared component but were not rebuilt in this trial.
- OTA binary SHA256:
  9788b23c6489b7d41fccb14c5942b5922277c1768b58cae95a8ecc3748358d53.

English, Classic Emoji, 180-degree rotation and all existing device settings and
local backups were untouched. The pre-existing intermittent white-flash issue
remains unresolved; this change makes no display-stability claim.

## Panel10 device validation, 2026-10-25

Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1, COM24, ESP-IDF 6.1; source
f7084ce3-dirty with the KNX implementation, built at 21:45:09.
The application-only update used the supported 32 MB flasher stub. Readback
matched the application, and metadata/NVS, retained core dump and LittleFS
checksums matched before and immediately after flashing. Startup ELF prefix
9ce3b1273, the running source/build stamp and OTA valid state were checked.
Application SHA256:
95d49e8059eca4d8e2b1ba8901b09564f172f83b4575b3f0a237755a9d89e643.

- The owner's updated 30-object WLED registry passed the production parser and
  was imported and saved through the device editor. Exported and saved JSON
  matched the supplied file. A controlled restart retained the registry and
  enabled state and reopened Routing.
- Invalid configuration, unknown objects and commands while disabled were
  rejected. Disabling preserved the registry; enabling restarted Routing.
  These negative checks sent no telegrams.
- A passive capture confirmed the panel's GroupValueRead for 2/0/1. That read
  initially had no feedback; it was not treated as confirmed actuator state.
- The authorized WLED power test wrote 1/0/1 and received fresh state on 2/0/1.
  The lamp changed from off to on and back to off. WLED's read-only JSON state
  independently confirmed both transitions and an exact full-state match after
  restoration, including brightness, color and segment settings. Other objects
  were not commanded. The editor displayed the valid received power value.
- The first voice trial produced speech but sent no KNX command. Review found
  that starting microphone capture discarded queued MCP handshake requests
  together with stale speech. The purge now services MCP messages, bounded to
  the queue's initial length; disconnect still discards the old session. The
  six host suites passed, including interleaved speech/initialize/tools-list
  envelopes. Method-only diagnostics expose handshake/call results without
  recording tool arguments, credentials or session identifiers.
- The repaired image was built at 22:23:55, application 6,724,640 bytes,
  ELF prefix 1fd567ead, application SHA256
  da54263e7c4b75b36e870ecc34be7fe7b3e0802aa4f3764c8dd951519ef7e088.
  Its app-only stub update again passed readback and protected-storage checks;
  the running image matched and OTA was valid.
- The voice retest logged successful initialize, tools/list and tools/call
  responses. Speech produced real power commands and fresh feedback; WLED's
  independent state confirmed the lamp turned on. The owner confirmed spoken
  on/off control, replies, idle auto-close and stable physical output.
- The owner additionally exercised color/effect commands. On request, the panel
  restored the pre-voice state through KNX: power off, brightness 51/255,
  RGB 255/0/72 and effect 0. WLED's complete JSON state then exactly matched the
  saved baseline, including every segment and other untouched field.
- The bounded voice observation recorded 48 state changes over 269 seconds
  without decreasing uptime. Final diagnostics at 383 seconds showed no
  retained crash, 20 sent / 36 received / zero rejected KNX telegrams and the
  assistant idle with local wake rearmed. This is not an all-day test or proof
  that the previously reported intermittent white flashes are resolved.

Final comparisons preserved all device settings (apart from live time/RSSI),
layout, themes, assistant preferences and the transcript font's size/SHA256.
English, Classic Emoji, 180-degree rotation and the original enabled local
wake setting remained. The saved KNX registry still matched the owner's latest
file and Routing remained enabled. Other DPTs and boards need separate trials.

Private configurations, flash backups, captures and test logs remain local and
ignored by Git. The registry import is an intentional storage change after the
flash checksum comparison.

## Unresolved white flashes

The owner reported intermittent full-screen white flashes on this Waveshare P4
before the KNX implementation, including while idle and with Live Display closed.
The f7084ce device trial still reproduced them. No root cause or verified fix has
been established; the KNX and MCP changes do not address display scanout.

The owner reported stable output during the bounded KNX voice trial above.
That observation and the absence of a new crash do not close the white-flash
issue. Keep it open for a separate display investigation; do not describe this
firmware as a fix for the flashes.

## Provenance

Reference: [xiaozhi-esp32 feat/knx_ip at 61fc9d0b](https://github.com/betamoojw/xiaozhi-esp32/tree/61fc9d0b619f39ed20356b8de2f9c0edd954472d/main/knx).
Adapted codecs/validation retain its MIT terms in main/knx/LICENSE.xiaozhi.
BETTA provides its own lifecycle, storage integration, guarded API and editor,
and validates Routing media parameters instead of silently ignoring them.

All manifests pin esp_knx_ip from
[esp32_knx_ip at 19059496](https://github.com/betamoojw/esp32_knx_ip/tree/19059496b9c8230cf7473f6b729d2176350d1eb6)
(the reference's v1.0.0 tag). This dependency has no explicit license file at
that revision; its terms are not replaced by the XiaoZhi MIT notice.
Device formatting avoids the C++ stream/locale runtime to fit existing OTA slots.
