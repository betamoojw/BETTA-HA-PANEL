<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 optional Home Assistant setup validation

## COM25 update on November 19, 2026

The Guition ESP32-S3-4848S040 at `192.168.71.77` ran the
`panels3` application on valid slot `ota_1`. Its old runtime stamp was
20:49:57 China Standard Time on boot 139. COM25 matched the previously
recorded board identity, ESP32-S3 revision 0.2 and 16 MiB flash.

Two authenticated reads and the 10,636-byte editor export agreed on
five pages and 13 widgets. The exported settings, custom resources,
KNX and camera configuration were captured alongside OTA status,
task and memory status, and the device and restart logs.
The layout SHA-256 was
`aa13870324a9cb46ecabd7fba863e6675cff40afb4797508ee2c0eee304044d7`.

Before writing, the serial tool matched the installed partition table
and both the active slot and the previous application digest. The
backup covered the first 128 KiB of flash, the coredump partition and
all 2,752,512 bytes of LittleFS. The local backup digests matched the
device digests. A host mount of the LittleFS copy returned exactly the
same layout pages as the API. The volume SHA-256 was
`6a289f0c9ca9f886f68b07965c549bd7d554031f0949f28c23484362aa597cd1`.
The volume had zero local graph payloads, so the trial provides
storage-retention evidence without a populated graph history sample.

The only write targeted the active application at `0x6A0000` at
230,400 baud. The 4,592,672-byte image readback matched the build with
SHA-256
`11cc14212c948e8eca7e34c913923e3704f2afbd47f095e281bb828c07e160ff`.
The tool confirmed identical digests for the bootloader, partition
table, NVS, OTA metadata, inactive application, coredump and LittleFS
before rebooting.

## Behavior on the panel

The 115,200-baud startup capture shows the bootloader selecting
`0x6A0000`, ESP-IDF v6.1 and ELF prefix `cf2ab829d`. Wi-Fi returned
to the original address, HTTP listened on port 80 and the Home
Assistant WebSocket authenticated. The API reported `v0.9.2-rc2`,
source `228dd77c-dirty`, runtime stamp 22:21:47 China Standard Time
and valid `ota_1` on boot 140. Closing COM25 left the same boot
running. The last authenticated check in that trial reached 332
seconds of uptime.

On a regular dashboard page, opening **+ Widget** sent
`GET /api/ha/light_entities?status_only=1` and returned `ready`.
The live settings response reported that the saved connection was
configured and setup was complete. Anonymous layout access returned
HTTP 401 before and after, and administrator sign-in and logout
were exercised. The final editor export matched apart from its
creation time. The layout, active theme and persistent settings
matched the backup after excluding live telemetry and the new
derived setup status field. The original configuration used English,
`Africa/Addis_Ababa`, 12-hour time, a 22:00 night start, a 60-second
flip-clock saver, Matte appearance and the `dark_v2` theme.

UART captured 303 seconds, including 53 LVGL lock warnings. The API
reported zero retained crashes, and the serial capture contained no
panic or failed assertion. SNTP started but exceeded the startup
wait, and the clock was still unsynchronized at the 332-second
check. The stored time zone and time-server settings matched the
baseline.

## First setup with isolated settings

The HTML, CSS and `js/app.1f4473d01ef7b69c.js` served by the panel
matched the built assets. Chromium ran those files on loopback with
a separate file-backed configuration and synthetic Home Assistant
responses. This exercised the shipped UI while preserving the
panel's configured system.

| Case | Result |
| --- | --- |
| First setup with a draft address and token | **Skip for now** sent only `skip_ha_setup: true`, kept the browser document open and entered the editor. |
| Restart | A new test service loaded the saved choice from disk. A fresh browser session opened the editor directly. |
| Missing credentials | **+ Widget** explained the missing URL and token. Selecting Light left the chooser open and sent no entity query. |
| Later configuration | The shortcut opened **Home Assistant & MQTT** and focused the URL. Saving submitted both synthetic fields and the normal automatic-restart option. |
| Connection failure and token rejection | The chooser displayed the corresponding messages and sent no entity query. |
| Recovery and retry | **Retry** retained the search term and returned the synthetic light after the connection became ready. |

The S3 host storage regression exercised the firmware's save and
reload logic, including preservation of the other settings and a
failed write followed by a successful skip. It passed with 34 focused
editor tests. A physical first boot with blank settings, a physical
restart after skipping and an actual change of Home Assistant
credentials remain outside the COM25 evidence.

## Clock investigation

On boot 140, Wi-Fi obtained its lease at 8.205 seconds, SNTP started
at 8.215 seconds and the eight-second startup wait expired at 16.215
seconds. Home Assistant authenticated after that. The S3 boot path
passes the same saved server to the same SNTP implementation after
network acquisition and before starting Home Assistant in both
versions. The pre-update and COM25 builds used byte-identical SDK
configurations.

The same Guition and saved time server had already shown a startup
SNTP timeout on November 8, before optional setup was implemented.
That run recorded valid time later. [Earlier COM18 validation](panels3-http-budget-validation.md)

A read-only review found the COM25 image still valid on boot 141.
The device reported a manual restart, and its clock was synchronized
at both 279 and 498 seconds of uptime. The saved NTP server, layout,
theme and login protection matched the COM25 trial. The time zone
was already `Asia/Shanghai` at the first review read. The second
read found differences only in the time zone and its derived
display fields.

The evidence supports an intermittent clock-sync condition that
predates optional setup. Which network stage failed on boot 140
remains undetermined. The review used the current device settings
as found.

## Review build and regression checks

Review reproduced two issues in host and browser tests. An invalid
value for `status_only` could start an entity query. An outstanding
connection reply could apply an earlier widget choice after the
search text changed. The API now rejects invalid values with HTTP
400, and a new search invalidates the pending choice.

All 661 editor tests and the discovery, storage and account tests
for S3 and P4 passed. ESP-IDF v6.1 rebuilt `panels3` with runtime
stamp 23:25:49 China Standard Time. The 4,592,752-byte application
has SHA-256
`d13a5db705d130fab4c41e0d830ecf4905818f5b99721bca8f277782bda1c925`.
Its SDK configuration and partition table matched the COM25 build.
The embedded `js/app.cf2fda844f46af0d.js` passed the isolated browser
flow using the local build. At the end of that review, COM25 still
ran the 22:21:47 image used in the earlier device trial.

Private backups, serial and HTTP captures, comparison reports and
isolated screenshots are saved in
`.local/validation/panels3-ha-optional-setup-com25-2026-11-19/`.
Read-only clock checks and the local build evidence are in its
`review/` subdirectory.

## COM25 retest from commit `1e8b1e3` on November 20, 2026

A clean checkout of `1e8b1e34a5bb9322986cb6a7e9c917ac0d40b7f9`
built `panels3` with ESP-IDF v6.1. The 4,593,360-byte
`v0.9.2-rc2` application has SHA-256
`ffc245461419ebf450e7a88aef23736d490ade52f0a4a34b0af7ca947b8f29a4`.
The generated SDK configuration and partition table matched the
previous build. The image leaves 33% of its application slot free.

Two authenticated reads and the editor export agreed on five pages and
13 widgets at `192.168.71.77`. The flash backup included the first
128 KiB, the full coredump and LittleFS partitions and the previous
application. Each backup matched the device digest. The mounted
LittleFS copy also matched the exported layout. The layout SHA-256
was `aa13870324a9cb46ecabd7fba863e6675cff40afb4797508ee2c0eee304044d7`.
The LittleFS volume contained zero local graph history payloads.

COM25 matched the ESP32-S3 revision 0.2, 16 MiB Guition. The saved
OTA metadata selected the same valid `ota_1` slot as the running
firmware reported. The recorded erase range, `0x6A0000` through
`0xB01FFF`, stayed within that slot. The readback digest matched the
archived 4,593,360-byte application. Before reboot, the bootloader,
partition table, NVS, OTA metadata, inactive slot, coredump and
LittleFS digests matched their pre-write values.

### Boot, first clock synchronization and retained data

The UART capture showed the bootloader loading `0x6A0000` and app
ELF hash prefix `14bc90960`. The device reported source `1e8b1e34`
and valid `ota_1` on boot 143. The UART application descriptor stamp
was 06:57:51. The API stamp was 07:02:22, which matched the compiled
`app_build_stamp` inside the same image. Both timestamps are China
Standard Time on November 20.

The panel obtained its original address and started SNTP about
eight seconds into application startup. UART recorded `Time synchronized`
within the startup wait before HTTP started, at 07:15:58.793 China
Standard Time. Authenticated checks reported `clock_valid: true`
at 180 and 280 seconds of uptime on the same boot.

The real editor's **+ Widget** check returned `ready` and the panel
authenticated to Home Assistant. Anonymous layout access returned
HTTP 401 and administrator sign-in and logout succeeded. The layout
and theme matched the baseline. Only the live time and Wi-Fi signal
fields changed in the settings response, and only the creation time
changed in the editor export. The saved settings retained English,
`Asia/Shanghai`, 12-hour time, a 60-second flip-clock saver, the
22:00 night start, Matte appearance and the `dark_v2` theme.
Closing COM25 left boot 143 running.

### Effect of the startup LVGL lock timeouts

All 53 adapter lock timeouts occurred from 07:16:03.390 to
07:16:05.430, a 2.040-second window immediately after the dashboard
layout loaded. At 07:16:05.370 the log recorded a deferred splash
hide when the display lock could not be acquired. LVGL removed the
splash at 07:16:06.326, 0.956 seconds after the deferral. The rest of
the 236.6-second UART capture contained zero further lock timeouts,
panics, failed assertions or watchdog faults.

Boot 143 reported both UI heartbeats younger than one second at 180,
189 and 280 seconds. A read-only follow-up at 737 seconds returned a
valid 480 × 480 LVGL PNG and a `ready` widget availability response
on the same boot. The clock remained valid, there were zero retained
crashes, and the layout, saved settings and login protection still
matched. The evidence shows a temporary splash delay followed by
normal operation in the tested UI and editor paths. Physical screen
timing and touch response during the timeout window remain unverified.

### Isolated setup and recovery checks

The HTML, CSS and `js/app.ae56aa8a53b097bc.js` served by COM25 matched
the flashed image. Chromium replayed them on loopback with a fresh,
file-backed test configuration and synthetic Home Assistant replies.
**Skip for now** discarded draft credentials and entered the editor
without a reload. A fresh browser context and a restarted test
service reopened the editor using the saved choice. The settings
page saved the synthetic WebSocket URL and long-lived access token.
**+ Widget** showed distinct messages for missing credentials,
disconnection and token rejection. Once the fixture connection
recovered, **Retry** preserved the search and returned the expected
entity. The chooser sent zero entity searches while unavailable.

The pre-commit review replayed the same panel-served assets with a
fresh isolated configuration and passed the flow again. All 661 web
tests and the S3 host discovery, runtime storage and login tests passed
in the original run. A physical first boot with blank settings and a
physical reboot after choosing **Skip for now** remain untested.
COM25 was the only available Guition, and the credential changes were
limited to the isolated service. The raw LittleFS comparison covers
storage retention. The backup contained no populated graph history.

The image manifest, partition digests, private backups, startup log
and isolated results are in
`.local/validation/panels3-optional-ha-1e8b1e3-com25-2026-11-20/`.
The `review/` subdirectory contains the evidence audit, follow-up
snapshot and isolated replay results.
