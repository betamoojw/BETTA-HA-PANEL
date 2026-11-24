<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Editor workspace validation

The 2026-11-23 COM24 trial verified the Settings Inspector correction on one
Waveshare LCD-X 10.1-inch panel. The webpage regression passed after recovery
from a startup failure. **Startup qualification failed:** the first boot stalled
in an I2C transaction and the UI watchdog restarted the application. The crash
report remains on the panel and in the local evidence archive.

## Image and preservation

| Item | Measured result |
| --- | --- |
| Device | Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1, Rev3.2, COM24, `192.168.71.76` |
| Source | `999c432e-dirty`, based on `999c432`, with the uncommitted workspace correction |
| Version / SDK | v0.9.2 / ESP-IDF v6.1.1, SDK commit `051e4c4eb20346e043cdd47bc388fde75ff8af63` |
| Application build stamp | `Nov 23 2026 23:24:18` |
| Application | 6,796,400 bytes; 19,344 bytes free in the existing 6,815,744-byte slot |
| Application SHA256 | `9830dca16b46a1152f9d8d51950d221952572ea904a6a526741990d42eee8510` |
| Matching ELF SHA256 | `9a3b977259c376ebbc4ba2b2d4a10bac7ee3bfcafd63be52acf02a394f01419f` |
| Written slot | Current `ota_0`, address `0x20000`; no partition-table change |
| Whole-slot readback SHA256 | `14651201ff226b76358a4cc3b0cbabb5ab3f22dfe83f0cf3c6999ae43dc6c588` |
| Final runtime | Expected source/build stamp, valid `ota_0`, safe mode off, HA authenticated |

The Windows build used the project wrapper with local
`-DCMAKE_NINJA_FORCE_RESPONSE_FILE=ON`, an active 6.1.1 SDK and the verified C6
artifact described in [Build and flash](build.md). The default host profile
pointed at 6.1.0; its initial configure was stopped before serial access. An old
bootloader cache and command-length failure were corrected locally. The earlier
root C6 file was archived and replaced with the verified dependency. No compiler
optimization, partition or maintained build configuration was changed.

The completed sdkconfig was byte-identical to the previous successful image
(SHA256 `8207d0c0485246695788e73265c2be404fce960acdda1577bb97bd6c8a482c19`).
The wrapper completed with exit 0. The embedded fingerprinted JavaScript was
checked for the pane-aware Inspector guard before flashing.

Before serial access, authenticated exports captured layout, settings, users,
themes, languages, camera/KNX configuration, voice preferences and diagnostics.
Anonymous layout access returned HTTP 401. A fresh export immediately before
flashing matched the initial export apart from local time and Wi-Fi signal.

The flasher verified the USB identity, P4 revision, MAC, supported 32 MiB flash
and installed partition table. It archived metadata/NVS, coredump, all
19,529,728 LittleFS bytes and the previous application slot. Flash digests and
the read-only LittleFS inventory verified the backups before the application
write. Whole-slot readback matched the new image. Metadata, coredump, LittleFS,
the inactive application and the unwritten slot tail matched their pre-write
digests **before firmware boot**.

Final HTTP comparisons retained all 9 pages and 41 widgets, accounts/login
protection, themes, languages and camera/KNX configuration. Saved settings
differed only at `time.local_time` and `wifi.rssi_dbm`. Voice configuration
fields also matched. English, Asia/Shanghai, 12-hour time, Glow, the flip-clock
saver, its 180-second idle delay, disabled night mode and its stored 23:00 start
were retained. The unchanged Save test returned `ok: true, rebooting: false`.

The backup contained no `.grph` files and the layout had no graph widgets.
Stored bytes were preserved, but no record-by-record chart-history comparison
was possible. Boot diagnostics and the new crash archive legitimately changed
persistent bytes after boot; this does not alter the pre-boot flash comparison.

## Webpage results and test boundaries

The shared renderer previously selected the page/widget Inspector from widget
selection alone. A layout response arriving after Settings restoration called
`renderAll()` and reversed the pane switch's hiding. The corrected renderer
also requires the Layout pane. It retains the selection while panels are hidden.

| Environment | Evidence and result |
| --- | --- |
| Node v24.15.0, Windows | Four new regressions passed; the original renderer failed three unwanted-visibility assertions. The complete web suite passed 677/677 with no skips. |
| Local Chrome, synthetic localhost API/auth | Original bug reproduced. Corrected source passed at 1280 and 390 px, including delayed loading, Settings switches and retained layout/selection. These are simulated panel responses. |
| Installed panel, real login and responses | Administrator login restored Settings; page and widget Inspectors stayed hidden before and after the real layout response. |
| Installed panel, controlled browser delay | The browser held the layout request for 1.952 seconds. Settings stayed selected and both layout Inspectors remained hidden on release. No response content was replaced. This is controlled timing, not a naturally observed network stall. |
| Installed panel, three workspace round trips | The widget Inspector returned only in Layout, with the same page, widget ID and binding. The serialized editor layout remained unchanged. |
| Installed panel, Settings sections and actions | Device/HA section switches and Reload passed. One Save was allowed only after every submitted field matched freshly read settings; no secret replacements were submitted. HTTP 200, no reboot. |
| Installed panel, browser sizes | Desktop and 390 px captures showed no Inspector leakage. No page errors or unexpected writes occurred. |

The first browser attempt could not load the stalled panel. A second attempt
hit a test-driver error while reading a login response body after automatic
navigation. The driver now verifies HTTP status and the authenticated session
after navigation. The final run passed; neither harness adjustment changed
firmware or bypassed authentication.

Two 1280 x 800 LVGL PNG snapshots after recovery showed the centered flip
clock, weekday/date, PM and advancing seconds. These are API snapshots; physical
LCD animation, first-frame latency, touch and wake were not measured in this run.

## Startup failure and recovery

The UART capture lasted **335.047533 seconds** from the authorized reset. The
first boot logged splash/Wi-Fi initialization at 9.150 s, clock sync/HTTP at
16.499 s, touch initialization at 16.500 s and splash exit at 19.326 s.

At firmware uptime about 52 s, the task watchdog reported that CPU1's idle task
was starved while `lvgl` was running. Repeated register samples located the PC
at `0x48293e64` / `0x48293e6e`. The matching ELF resolves these to
`s_i2c_send_commands()`, `esp_driver_i2c/i2c_master.c:594–595`, in the NACK
bus-busy wait. Camera SCCB transmit/STREAMON failures occurred during the same
interval. These observations do not establish which peripheral caused the NACK.

At about 135 s uptime, the application UI watchdog reported 96 s without LVGL
progress and deliberately called `abort()` to preserve a core dump. The matching
ELF resolves `0x480d1f11` to `ui_watchdog_task()`, line 96. The reported illegal
instruction is part of this deliberate abort, not proof of memory corruption.

The new retained report is
`3427c705fd7c8e63d911d5aa9fdfa561d296dcdd80e65086300da4f458d6cffb`
(35,488 bytes). Its SHA256 was verified on download. Core decoding confirms the
watchdog abort; the installed decoder did not recover reliable other-thread
names/stacks. The peripheral caller and original I2C failure remain unresolved.
The follow-up below recovered the peripheral caller directly from GDB threads.
Crash metadata uses the app-descriptor time `23:23:46`; `/api/version` uses the
application build stamp `23:24:18`. Both belong to the same matched ELF.

The automatic restart loaded the same `ota_0` at capture time 140.216 s and
completed splash exit at 156.894 s. Boot sequence changed 251 → 252 → 253.
The final HTTP snapshot reported boot 253 at 432 s uptime, the expected image,
valid OTA state and no safe mode. There was no OTA rollback. The observed second
boot recovered, but this does not qualify startup stability.

The capture contained zero `Failed to acquire LVGL lock` messages and one UI
watchdog restart. A zero lock-timeout count must not be presented as a clean
startup result. No I2C/driver fix or second firmware flash was made in this trial.

## Evidence and remaining coverage

The ignored local archive is
`.local/validation/panel10-settings-inspector-com24-2026-11-23/`.
`image/manifest.json` pins the image, ELF, configuration, source files and web
assets. `flash-result.json`, `serial/backup.json`, raw backups and pre/post
checksums cover the write. `serial/boot.log`, `serial/events.ndjson`,
`serial/capture.json`, `after-panic/` and `coredump-decoded.log` retain the failure.
`browser-workspace.json`, desktop/narrow browser captures, `runtime-check.json`
and `audit.json` cover the recovered run and data comparisons. Raw backups and
crash data contain private device information and remain outside Git.

The offline audit checks evidence consistency and explicitly records startup
qualification as false. At the end of the 2026-11-23 trial, the peripheral caller
and bounded recovery were unresolved. Physical LCD/touch confirmation,
repeated-start qualification and chart-history samples were also outstanding.
No commit or push was performed.

## I2C stall follow-up

On 2026-11-24, offline analysis recovered the `lvgl` thread directly from the
same core ELF and matching application ELF. The high-level core decoder's task
enumeration was unreliable; direct GDB thread backtraces and `TCB_t` reads
identified TCB `0x4ff717e8`, name `lvgl`, priority 20. No device access was needed.

The actual blocked operation was a one-byte read of GT9271/GT911 touch status
register `0x814e`, using address bytes `0x81 0x4e`. Its call chain was:

```text
lvgl_worker -> lv_timer_handler -> lv_timer_exec -> lv_indev_read
 -> lvgl_touch_read -> esp_lcd_touch_gt911_read_data
 -> touch_gt911_i2c_read -> panel_io_i2c_rx_buffer
 -> i2c_master_transmit_receive -> s_i2c_transaction_start
 -> s_i2c_send_commands
```

GDB recovered `xfer_timeout_ms=-1`, `event=I2C_EVENT_NACK`,
`ticks_to_wait=timeout_ticks=4294967295` and `start_tick=39135`. The LCD-X BSP
left `transaction_timeout_ms` at zero. IDF 6.1.1 maps zero to `-1`, then to
`portMAX_DELAY`. Its NACK path waits while the bus is busy and tests an unsigned
tick difference `> timeout_ticks`. No unsigned difference can exceed UINT32_MAX,
so this particular wait cannot expire with the inherited setting.

The matched worker disassembly acquires the adapter mutex at `0x481b8128`,
calls `lv_timer_handler` at `0x481b812e` and unlocks at `0x481b8134`. The stalled
input therefore retained the LVGL lock as well as the driver's I2C bus lock.
The camera uses the same I2C1/GPIO7/8 handle, with transactions serialized by
the SDK. At the crash, `ambient_cam` was in `vTaskDelay()` at `ambient_task:529`.
Concurrent SCCB errors do not establish that the camera caused the initial NACK.
Its original electrical/peripheral trigger remains unknown.

The source correction is restricted to LCD-X/Panel10:

- Configure the touch IO with a finite 50 ms IDF timeout. IDF uses this budget
  in multiple sequential stages, including bus admission; this is not a measured
  50 ms end-to-end LVGL callback deadline. Its existing timeout path resets the
  transaction FSM and releases the bus mutex. The shared bus is not recreated.
- Return unknown input on a failed scan. The existing adapter policy holds up
  to three failed scans, then cancels a held gesture and suppresses the contact
  until a valid release. A timeout does not fabricate a click.
- Release the panel IO on controller-construction failure and both controller
  and IO on later binding failure. Display lookup, input registration and timer
  tuning all run under the display lock; initialization retries run outside it.

Other hardware keeps its existing BSP and touch initialization. The shared SDK,
adapter retry policy, GPIO/reset/probe/rotation configuration and persisted
settings are unchanged. No additional task, stack or frame buffer was added.

### Local checks and new image

| Check | Result |
| --- | --- |
| Production BSP + IDF fault injection | 3 Python cases compile and execute C paths, all passed, no skips; NACK, busy bus, command/queue timeout, bus admission, FSM/mutex cleanup, recovery, constructor allocation failures and gesture cancellation. Includes legacy IO defaults on other devices. |
| Existing touch logic | 86 checks, no failures. |
| Ambient camera policy / actual worker with stubs | Both passed, including shared-bus configuration, expiry, allocation cleanup, sensing and failures. A conflicting host-only error macro was guarded to enable the worker check; production camera code is unchanged. |
| LVGL adapter locks | 5 tests passed, recursion, timeout and recovery retained. |
| Settings Inspector | 4 webpage regressions passed; web consistency check passed. |
| Source and documentation size / whitespace | Passed, existing advisory warnings retained. |

The injected tests compile actual BSP/IDF transfer and error-cleanup functions
against controlled HAL/RTOS stubs. They do not emulate electrical timing. Tool
check cache inputs include the new fixtures and the production touch sources.
The IDF-dependent case skips if no SDK is supplied; this local run used 6.1.1
and executed every case with strict C warnings enabled.

The project wrapper built only `panel10`, ESP-IDF 6.1.1, source `999c432e-dirty`,
with application stamp **Nov 24 2026 22:04:08**. The local incremental build
initially reused an old build-stamp object; that generated object was removed
and rebuilt through the wrapper before packaging. No source workaround or SDK
patch was needed. The new image is 6,796,320 bytes, leaving 19,424 bytes in the
unchanged OTA slot. SDK configuration SHA256 remains the same as the pre-fix
image above; the Settings correction's web assets also match exactly.

- Application SHA256: `94654d4cc814b726400317a07855035be11ad4fd43ed45879788e2d043932175`.
- ELF SHA256: `7c661ccb370b7b566ab1ca0b419041b1e32dfb4db59988085b30dba992283cbc`.

The new local evidence is
`.local/validation/panel10-i2c-stall-2026-11-24/`.
`evidence/RESULTS.md`, `inspect.log`, `tasks.log` and `tcb.log` retain the direct
GDB commands and recovered values. The original COM24 report remains the
**pre-fix** hardware evidence. At the end of this offline follow-up, the new
image was build-only: NACK recovery,
gesture cancellation, camera coexistence and repeated starts still require
authorized hardware validation. No restart, flash, report deletion, commit or
push was performed during this follow-up.

The subsequent authorized COM24 trial of this image is recorded separately in
[Panel10 I2C validation](panel10-i2c-validation.md), with its own hardware
observations and remaining coverage limits.
