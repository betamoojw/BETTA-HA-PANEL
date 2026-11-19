<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 regional date validation

## Guition baseline, November 19, 2026

The Guition ESP32-S3-4848S040 at `192.168.71.77` reported the 480 × 480
`panels3` application on valid slot `ota_1`. The running build was
`43d96a61`, November 18 at 23:16:42. Boot sequence 135 had no retained
crash reports. The host listed COM18.

Two authenticated reads agreed on all five pages and 13 widgets. The raw
layout was 3,186 bytes, SHA-256
`aa13870324a9cb46ecabd7fba863e6675cff40afb4797508ee2c0eee304044d7`.
The captures include settings, the active theme, task and memory status,
device and reboot logs, and OTA status. Anonymous layout access returned
HTTP 401. The two settings reads agreed after excluding signal strength
and the ticking local clock.

Chrome, set to Honolulu, also downloaded the panel editor's own 10,634-byte
backup. The exported layout matched both HTTP reads. The export includes
settings, themes, languages, KNX and camera configuration. After logout,
the same browser again received HTTP 401 for the layout.

The panel used English, `America/New_York`, 12-hour time, a 22:00 night
start, the `dark_v2` theme and Matte appearance. Its top bar included
the date. The flip-clock saver was set to 60 seconds. The saved pages
contained no graph, date/time, or timestamp sensor widgets.

## COM18 installation and startup

The installed ESP-IDF v6.1 application is `v0.9.2-rc2`, source
`43d96a61-dirty`, built November 19 at 00:22:51. Its 4,589,104 bytes
have SHA-256 `1280ba34715f84578b4d0c65b1620b60d9d0755981dbabdf10d50436f6bf2f6f`.
The ELF matches the digest embedded in the image. The image also
contains the checked editor bundle `js/app.407e33a00a744bd9.js`.

After the owner authorized COM18, a 460,800 baud read returned a storage
packet two bytes short. Verification stopped before writing. The panel rebooted
into the previous application and its saved layout and configuration
matched the earlier snapshots. The retry used 230,400 baud and checked
256 KiB read segments. It verified the installed partition table, the
active valid `ota_1` slot, the previous application image and a
2,752,512-byte, mountable LittleFS backup. LittleFS had no `/graphs`
directory or graph payload. Its stored layout matched the HTTP page model.
The storage backup SHA-256 is
`c6632820d3958ceabadd989ab4bf931a591d3aea6f242b6b5323fb927a7c84d4`.

The only flash write was the application image at `0x6A0000` in `ota_1`.
The device verified the written image. Its pre- and post-write digests
matched for the first 128 KiB (including bootloader, partition table, NVS
and OTA metadata), the crash partition, inactive `ota_0` and LittleFS.
COM18 remained open on the same serial handle for startup logging.

UART identifies ELF `fab283a59` and shows the previous New York time zone
applied at startup, time synchronization, and HTTP listening on port 80.
The ESP-IDF app descriptor printed November 17 at 20:34:57. The running
`/api/version` source stamp was November 19 at 00:22:51, matched the archived
binary and reported valid `ota_1` at boot sequence 137. The editor served
the expected bundle from the device with the archived decoded SHA-256.
The five-page layout, authentication and original time preferences
were present immediately after boot. UART logged 53 LVGL lock waits in
initialization, all before the regional date tests.

## Dates on the installed application

Chrome used `Pacific/Honolulu` and loaded the editor from the panel.
The time-zone picker saved Berlin, Tokyo and New York on the same boot.
Each save returned HTTP 200 with the new zone already active and
`rebooting: false`. UART recorded all three changes. The timestamps below
were collected from the device around the corresponding screenshots.

| Selected zone | Editor's current-clock preview | Panel top-bar date | Panel flip saver |
| --- | --- | --- | --- |
| Europe/Berlin | 19.11.2026 01:08:28 PM | 19.11.2026 | Thursday, 19.11.2026 at 01:09:40 PM |
| Asia/Tokyo | 2026/11/19 09:09:41 PM | 2026/11/19 | Thursday, 2026/11/19 at 09:10:50 PM |
| America/New_York | 11/19/2026 07:10:51 AM | 11/19/2026 | Thursday, 11/19/2026 at 07:11:59 AM |

All three dashboard and saver pairs came from authenticated 480 × 480
device PNG captures. The original 60-second idle delay remained in use.
The owner observed the physical display and confirmed the regional
dates, weekday and AM/PM in all three zones. The centered saver
filled the screen without the dashboard header.

The served editor also formatted a supplied `2026-01-01T00:30:00Z` and
the civil date `2026-01-01` in each zone. Its timestamp and weekday moved
back to Wednesday, December 31 in New York, while the civil date
remained January 1 in every zone. The inputs were test values given to
the loaded preview functions. No date/time or timestamp tile was present
on the installed dashboard.

## Native settings

The owner used the physical panel's **System settings → Time zone** and
confirmed the date display after saving Berlin, Tokyo and New York.
UART recorded those zones at about 20:24:24, 20:24:50 and 20:25:01
China Standard Time on boot 137. The physical test followed inconclusive
attempts to scroll the long native list using Live Display.

A read-only Honolulu browser observed the running editor update its
current clock and active-zone label after native changes. After Tokyo
changed to New York, the active label became `America/New_York` and
the clock showed `11/19/2026 07:25:05 AM`. The already-open selector
still read `Asia/Tokyo`. A fresh Settings load reads the saved zone.
The final restoration also updated the clock before the picker text
changed. A page reload showed both the New York picker value and date.

## Simulator and browser checks

The 480 × 480 simulator rendered a Home Assistant timestamp of
`2026-01-01T00:30:00Z` alongside a date-only entity of `2026-01-01`.
The table gives the sensor and date widget labels read from LVGL.
The weekday column came from the corresponding browser regression.

| Selected zone | Timestamp sensor | Weekday | Date-only widget |
| --- | --- | --- | --- |
| America/New_York | 12/31/2025 19:30 | Wednesday | 01/01/2026 |
| Europe/Berlin | 01.01.2026 01:30 | Thursday | 01.01.2026 |
| Asia/Tokyo | 2026/01/01 09:30 | Thursday | 2026/01/01 |

All 18 simulator layouts passed content and geometry checks. The 480 × 480
clock and page-transition regressions passed. All eight focused web tests
passed with the host set to Honolulu, covering cross-midnight dates,
weekdays, summer time, stale clock replies and live time-zone selection.

In a separate local Chrome session set to Honolulu, the shared editor
displayed the table's timestamp, date-only value and weekday after each of
the three picker selections. Its current-clock preview also matched the
local server's civil time in 12-hour format. This run used the panel10 demo
fixture because the stock S3 editor fixture failed to initialize its
`weather_3day` example. The 18 LVGL layouts above used the S3 profile.

## Restoration and data checks

The final restore sent only `time.timezone = "America/New_York"` with
`reboot: false`, at 20:31:12 China Standard Time. The authenticated
settings response and the fresh editor both showed New York. All final
persistent settings and the active theme matched the pre-flash snapshot.
The original and final editor exports matched after removing the export
timestamp. The 3,186-byte layout matched byte for byte. English, Matte,
`dark_v2`, 12-hour format, the 22:00 night start and the 60-second flip
saver all matched the original settings.

There were no populated graph files in the verified backup. The device
verified LittleFS across the flash and the UART trace showed no nonzero
graph payload writes during the time-zone tests. A populated history
comparison requires a device with saved graph samples.

The serial recording covers 1,499 seconds on the original COM18 handle.
After the handle closed, the same image was valid at 1,524 seconds of
uptime. The final authenticated check remained on boot 137 with zero
retained crashes, and anonymous layout access returned HTTP 401.

The device captures, browser screenshots, partition digests and
`boot.log` are archived locally under
`.local/validation/panels3-timezone-dates-com18-2026-11-19/`.

## Editor time-zone synchronization

The clock read now updates the picker's selected value and the date preview
from the same device response. When the list is open, it preserves the typed
query, caret and keyboard highlight. Escape shows the latest applied zone.
The editor discards results older than the applied state and serializes
time-zone writes so a fast sequence ends at the last choice. Saving appearance
or the time server leaves the zone on its dedicated save path.

Twenty-two focused Node tests and all 651 web tests passed. The regressions
cover late clock and full-settings reads, fast browser choices, save failure
recovery and blocking unrelated saves from sending a stale zone.
Chromium in Honolulu used the localhost editor with the panel10 demo fixture.
It observed New York, Berlin and Tokyo on the same page without saving any
settings, then retained the active search and selection range through a rapid
change. The same test checked cross-midnight previews, the existing clock
options and the fixture's unchanged layout.

ESP-IDF v6.1 built the updated `panels3` application on November 19 at
20:49:57 China Standard Time, source `43d96a61-dirty`, version
`v0.9.2-rc2`. A second incremental pass refreshed the runtime build
stamp after the editor changed. The 4,589,536 byte application has SHA-256
`f7e27b66b11404fc3664a4c80d7dffba902417343c614b0d39d3bb7a95094c19`.
The embedded `js/app.848d5f2d68eee9ad.js` contains the checked editor
sources. The ELF digest matches the image and the SDK configuration and
partition table match the earlier COM18 build byte for byte.

The preceding COM18 observations apply to the 00:22:51 build. Local test
logs, the browser report and the
new image manifest are archived under `.local/validation/panels3-timezone-picker-sync-2026-11-19/`.

## COM18 check of the 20:49:57 image

The same Guition at `192.168.71.77` reported valid `ota_1` on boot 137
before the update. Two authenticated reads and the 10,634-byte editor
export agreed on five pages and 13 widgets. The 2,752,512-byte LittleFS
backup mounted and its layout matched the HTTP page model. It contained
no graph files. Its SHA-256 is
`ec86354cb407bf0e814aae4e5cf1ea0220fcece653372d8ca9ccc423e7870f1e`.

At 230,400 baud COM18 wrote only the verified application at
`0x6A0000`. Readback matched the 4,589,536-byte image. The rest of
flash retained the same digests for the bootloader and partition
table, NVS, OTA metadata, inactive slot, coredump and LittleFS.
UART identified ELF `2a700cf18` and recorded Wi-Fi, time synchronization
and HTTP startup. The running API reported the 20:49:57 source build,
valid `ota_1`, boot 138 and zero retained crashes.

### Open editor sync on the device

Physical input was unavailable during the automated session. Live Display
opened the native Time zone list and relayed a swipe. The first visible
rows stayed in place, so the automated session used the settings API.
An intervening Caracas setting appeared in UART before the browser
trial. The test restored New York before collecting its baseline.

The fallback used the real panel's time-only settings save, with
`reboot: false`, while a Chromium tab in Honolulu stayed on
**Language & time**. Each save returned HTTP 200. UART recorded the
changes and the open tab accepted the following clock snapshots without
navigation or writing settings. Capture times include waiting for the
next poll and taking the browser screenshot.

| Zone | UART arrival, China Standard Time | Editor current clock | Save to capture |
| --- | --- | --- | --- |
| Europe/Berlin | 21:17:17.291 | 19.11.2026 02:17:22 PM | 5.73 s |
| Asia/Tokyo | 21:17:23.442 | 2026/11/19 10:17:28 PM | 5.77 s |
| America/New_York | 21:17:29.720 | 11/19/2026 08:17:34 AM | 5.48 s |

The Tokyo change arrived during a search for `toky`. The focused input,
selection range 1–3 and keyboard-highlighted option were retained. That
option's selected state changed to true. Escape showed `Asia/Tokyo`.

For a controlled overlap, the browser held a clock request and accepted a
newer full settings response from the panel in New York. It then received
the real Tokyo reply saved earlier in the session. The trace records the
older request 6 reaching the clock handler after request 7 was applied.
The handler kept request 7, the New York date, and an open `berl` search.
Escape showed `America/New_York`. This replay completed in 409 ms on boot
138, with no page navigation, browser errors or editor settings writes.

### Physical menu follow-up

On November 19 the owner confirmed that changing the time zone from
the Guition panel's physical System settings worked normally on the
installed 20:49:57 image. The browser timings and delayed-response trace
above were captured during the automated settings API run.

### Final state

The saved and active zone returned to `America/New_York`. The final
3,186-byte layout, persistent settings and active theme matched their
pre-flash reads. The two editor exports matched apart from the export
timestamp. English, 12-hour time, the 22:00 night start and the original
flip saver were preserved. Logout again left layout access at HTTP 401.
LittleFS was byte-verified across the flash. Its backup had no populated
graph history and the capture contained no nonzero graph payload write.

The serial capture covers 970 seconds. It contains 56 LVGL lock warnings,
the last at 21:09:18 China Standard Time before the automated sync
sequence. Closing COM18 left the same valid image running at 1,002 seconds
of uptime. There were zero retained crashes. The serial log, before/after
digests, browser screenshots and replay trace are in
`.local/validation/panels3-timezone-picker-sync-com18-2026-11-19/`.
