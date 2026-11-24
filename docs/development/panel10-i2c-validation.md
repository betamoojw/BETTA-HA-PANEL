<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Panel10 I2C validation

COM24 hardware validation on 2026-11-24 of the bounded touch-I2C correction,
using the existing Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 LCD-X Rev3.2 panel
at `192.168.71.76`. The root cause and host fault-injection coverage are in
[Editor workspace validation](editor-workspace-validation.md#i2c-stall-follow-up).

## Firmware and baseline

The installed candidate is the previously built `panel10` image, application
stamp **Nov 24 2026 22:04:08**, source `999c432e-dirty`, v0.9.2, ESP-IDF 6.1.1.
It includes the Settings Inspector correction. No rebuild was performed for
this trial; the manifest's production-source and artifact hashes were checked.

- Application SHA256: `94654d4cc814b726400317a07855035be11ad4fd43ed45879788e2d043932175`.
- Matching ELF SHA256: `7c661ccb370b7b566ab1ca0b419041b1e32dfb4db59988085b30dba992283cbc`.
- Application size: 6,796,320 bytes; unchanged app slot: 6,815,744 bytes.

Before opening COM24, authenticated HTTP snapshots archived the layout,
settings, users, themes, languages, camera/KNX configuration, voice settings,
version/tasks, reboot history, OTA state and bounded log. Anonymous layout
access returned HTTP 401. The baseline was boot 256, valid `ota_0`, running
the **Nov 23 2026 23:24:18** application.

Both retained crash payloads were downloaded by ID and verified by SHA256:

- `e8799e408f4523523eb1153881b09dc66a29330db03b7d28be488c11284eaccf`.
- `6c527973acb732c21b187f8b06a602790d9db520a08a379210beaecbede34eeb`.

Their matching pre-fix ELF is `9a3b977259c376ebbc4ba2b2d4a10bac7ee3bfcafd63be52acf02a394f01419f`.
Offline GDB again located the LVGL touch read at GT911-compatible status
register `0x814e`, reaching `s_i2c_send_commands` with `ticks_to_wait=4294967295`.
These are pre-flash failures, not crashes of the corrected image. The reboot
history records previous runs of 128 and 1,484 seconds, in addition to the
original 135-second failure. No retained report was deleted.

The baseline has 9 pages and 41 widgets. Its settings are English,
`Asia/Shanghai`, 12-hour time, Glow, a flip-clock saver after 180 seconds,
and night mode disabled with a stored 23:00 start. Local camera output,
stream permission and advanced motion detection are already enabled;
the trial does not enable them or change any settings.

## Backup and application write

The identified ESP32-P4 flasher stub verified the installed partition table
and archived metadata/NVS, the 256 KiB coredump partition, all 19,529,728 bytes
of LittleFS and the complete previous application slot. Archive hashes were
checked against flash digests before writing. LittleFS was mounted read-only
on the host: the layout is 10,392 bytes and settings 1,472 bytes.

The backup contains **no `.grph` history files**, and the layout has no graph
widgets. Full storage preservation can be checked, but there are no chart
history samples for a per-record comparison.

Only `ota_0` at `0x20000` was written. The complete slot was read back before
booting: SHA256 `d4bba80a67a69d07c737c77f9ffdd06b41cda94c01adea1dbb149d2475adb38a`.
The application prefix exactly matches the pinned image. Metadata/NVS,
coredump, LittleFS, the inactive slot and the unwritten active-slot tail all
matched their pre-write digests. No partition table, bootloader, OTA selection
or device setting was written by the test tools.

## Startup and shared-bus observation

The serial capture lasted **365.017 seconds**, starting immediately before
the authorized reset. It contains one application load from `0x20000`, the
matching ELF prefix `7c661ccb3`, and no subsequent reset. HTTP confirmed the
new application stamp, boot 257, valid OTA state and safe mode disabled.
There is no rollback evidence.

| Event | Capture-relative time |
| --- | --- |
| Application loaded | 1.368 s |
| Splash / Wi-Fi initialization | 9.166 s |
| First network time synchronization and HTTP service | 19.047 s |
| Touch initialized | 19.048 s |
| Splash hidden | 21.834 s |
| OV5647 ready, 800 × 640 | 28.415 s |

These are software/log timings from the host's reset/capture start, not an
optically measured LCD first frame. The owner confirmed the Wi-Fi prompt and
normal transition to the dashboard. No video timing was taken.

The capture contains **zero** I2C NACK, I2C timeout, SCCB/STREAMON failure,
LVGL lock-timeout, task-watchdog trigger, UI-watchdog stall or panic/restart
messages. Eight successful status samples between capture seconds 40 and 300
reported zero-second LVGL/runtime heartbeat ages, camera ready/not failed and
valid motion sensing. Samples at 120, 135 and 150 seconds bracket the original
135-second failure. The earlier 128-second failure is covered too.

**Sampling exception:** the ninth HTTP status request, scheduled at capture
second 360 with a four-second client timeout, raised `TimeoutError`. The
serial capture contains no matching bus/watchdog failure. A subsequent full
snapshot at device uptime 383 seconds succeeded on the same boot with both
heartbeat ages zero. The cause of that individual HTTP timeout is unresolved;
the record does not describe all nine polls as successful or infer a Wi-Fi
disconnect from it.

Advanced motion sensing remained enabled throughout. Five additional status
reads at uptimes 238–299 seconds reported camera ready/not failed and live UI
heartbeats. The output sequence was zero while no image client was consuming
frames; it is not a motion-sampling counter. Later, two ordinary authenticated
camera snapshot reads at uptimes 1,072 and 1,076 seconds succeeded (57,113 and
54,157 bytes), with output sequence 1 → 6 and both heartbeat ages still zero.
Only frame hashes and status were kept, not indoor camera imagery.

The log also contains two hardware JPEG-header decode errors followed by the
existing software-decoder fallback, and periodic HA energy-preference request
warnings. These belong to HA cover/preferences handling, not the local camera
or touch-I2C path. They were recorded without expanding this trial into fixes
for those separate paths.

## Editor and preservation checks

Chrome loaded the installed editor directly from the panel and logged in as
the existing administrator. A browser-local saved location restored Settings.
The test deliberately held the **real** layout response for 1.983 seconds:
before, during and after that delay, both Layout inspector cards stayed hidden
and the Settings actions stayed visible. Three Layout/Settings round trips,
explicit rerender, Device/HA section selection and Reload Settings passed.
Returning to Layout retained the page, widget ID and selected widget content.
The loaded layout model was unchanged; there were no browser page errors.

Desktop (1280-pixel) and narrow (390-pixel) browser captures were saved. This
is a real-panel browser check with a controlled response delay, not proof of
a naturally occurring two-second network delay. No response data was mocked.
The Save action remained visible; no settings save was sent in this trial.
Earlier save-path and synthetic/local regression coverage remains in the
[preceding record](editor-workspace-validation.md).

The first screenshots at uptimes 223 and 226 seconds showed an open Wi-Fi
information popup, **not the screensaver**. The owner confirmed opening it
manually, closed it and let the unchanged 180-second idle interval elapse.
Those initial images are retained as popup evidence and not counted as a
successful flip-clock capture.

After that interval, two 1280 × 800 LVGL snapshots at uptimes 1,332 and
1,335 seconds show the centered flip clock, Tuesday, `2026/11/24` and PM,
with seconds advancing from 53 to 55. The owner separately confirmed normal
physical flipping, wake touch and page switching, without unintended control
activation. Software snapshots alone do not establish physical display/touch
behavior. Camera sensing remained enabled during these checks.

The final authenticated snapshot at uptime **1,404 seconds** still reported
boot 257, the pinned firmware, valid `ota_0`, HA authenticated and no additional
crash IDs. Both downloaded retained crash payloads exactly match their
pre-flash copies. The editor's warning banner refers to those old reports;
it is not evidence of a new crash in this trial.

Page/widget content, user/login protection, theme collections, languages,
camera/KNX configuration, license and voice configuration compared equal.
The final settings differed only in live `time.local_time`; the intermediate
post-window comparison also recorded a live `wifi.rssi_dbm` change. The
English/Shanghai/12-hour/Glow/180-second flip-clock configuration and stored
night settings above are unchanged. Only the authorized reset advanced the
boot counter. The final read-only check is additional evidence, not an
extension of the continuously recorded 365-second serial interval.

## Coverage limits

- No NACK, persistent-bus-busy condition or transfer timeout occurred in this
  hardware window. The finite wait, FSM/mutex release, gesture cancellation,
  allocation cleanup and post-fault refresh paths remain covered by the
  earlier host fault-injection tests, not by an induced physical bus fault.
- The configured 50 ms timeout is an IDF per-wait budget, not a measured
  end-to-end touch callback bound. No bus analyzer or electrical fault
  injection was used; the original NACK's physical cause remains unknown.
- This is one authorized start. It is not repeated-start or long-duration
  qualification and does not qualify other hardware variants.
- The isolated four-second HTTP sampling timeout remains unexplained.
- There were no per-record chart-history samples to compare.

The storage-preservation helper's 8 local regressions passed before the write.
Existing source/firmware regressions were not rerun because the exact archived
image was used without production changes. No firmware rebuild, crash-report
deletion, commit or push was performed.

## Evidence location

Private raw evidence remains in
`.local/validation/panel10-i2c-com24-2026-11-24/`.
It includes the pinned image/ELF, HTTP snapshots, retained crash payloads and
GDB stacks, raw partition backups, flash/readback results and serial logs.
It is excluded from Git because it contains device data.

`audit.json` checks the pinned image, whole-slot readback, protected-region
digests, logical comparisons, retained crash payloads and browser results.
`runtime-samples.json` preserves the failed 360-second poll as well as the eight
successful samples; `camera-output-check.json` covers the later actual frames.
`owner-confirmations.json`, `final-saver/` and `browser-workspace.json` separate
owner observations, LVGL captures and controlled browser checks. The older
`saver/` captures show the Wi-Fi popup and must not be reused as clock evidence.
