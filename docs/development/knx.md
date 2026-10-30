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

Configuration and enable changes increment a generation. Queued commands must
match that generation and the current permissions before sending. Old requests
cannot write after a registry replacement or disable.
Incoming self-address telegrams are ignored; no optimistic state confirmation.
Caches live only in RAM with monotonic last_update_ms timestamps.

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

Commands return HTTP 202 with queued=true, completion=asynchronous and
bus_confirmed=false. Queue/send rejection counts and transport errors appear in
status. No raw APDU or arbitrary-address endpoint is exposed.
Normal editor backups add optional knx.configuration and knx.enabled.

## Xiaozhi MCP

The existing WebSocket hello advertises MCP. JSON-RPC initialize, ping,
tools/list and tools/call run on the Xiaozhi worker; commands enqueue without
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
Real Routing reception, reconnect, voice-to-actuator operation and concurrent
audio/display stability require a separate authorized device trial.

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
