<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk | Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Release notes

Changes of **this fork**, compared with upstream v0.8.2. The release notes of the original project (v0.8.2 and earlier, by Cpt_Kirk) are kept unchanged in [Upstream release notes](release-notes-upstream.md).

## v0.9.1

Changes since [v0.9.0](#v090).

The development branch combines these updates with its existing S3/LCD-X support, seven-language catalogs,
Live Display, native settings, clocks and animations, Xiaozhi, KNX and saved camera sources. Existing translations
are retained; new upstream labels fall back to English until their additional-language translations are supplied.
Late weather detail responses are discarded safely after their popup closes, including responses already queued for the UI task.
The combined `panel10` build fits its existing 6.5 MiB OTA slot by losslessly compressing the bundled C6 updater.
All features and the partition layout are retained; the restored C6 image is verified before transfer.

### Xiaozhi AI

- The web editor can save a custom Chinese wake phrase as pinyin, with local
  validation, restart persistence and reset to defaults. A separately installed,
  checksum-verified MultiNet resource avoids changing the flash partitions.
- Ni hao Xiao Zhi, Hi ESP and Jarvis presets are also selectable. Disabling the
  assistant retains the selected phrase; custom input failures retain the previous choice.

### Widgets: new and changed

**New** means a type added since v0.9.0; **Changed** means an existing type was updated. The Parqet portfolio, Stocks & ETFs and
Energy price forecast tiles share one option, **Show selector buttons on the tile**. It is on by default for Parqet and Stocks; the price
forecast keeps it off, so it shows the running 24 hours until the buttons are switched on.

| Widget | Status | What is new or changed |
| --- | --- | --- |
| [Energy price forecast](widgets/energy_price_forecast.md) | <span class="release-status release-status--new">New</span> | A day of electricity prices as hourly or 15-minute bars from a price sensor (EPEX Spot, Nord Pool, Tibber). The price now, Today/Tomorrow buttons, the day's minimum and maximum and the next cheap window. |
| [Parqet portfolio](widgets/parqet_portfolio.md) | <span class="release-status release-status--new">New</span> | Value, gain, return, XIRR, dividends and the largest holdings from the Parqet Companion sensors, with tabs and interval buttons on the tile; an option hides amounts. |
| [Stocks & ETFs](widgets/stock_quotes.md) | <span class="release-status release-status--new">New</span> | Up to eight Easy Stock quotes as a list, as charts or as tiles (a card per quote with price, change and chart, two columns on wide tiles, one on narrow ones) with period buttons (1D to 1Y). A tap opens the details with a chart; the top bar's stock item opens its price history. |
| [Sensor](widgets/sensor.md) | <span class="release-status release-status--changed">Changed</span> | The value follows the device class: local time, dates, durations, scaled units, currency with the entity's display precision, readable states and the decimal comma. A new **List** view shows up to eight sensor and binary-sensor entities, each with its own formatting, availability and history on tap; the Inspector adds, orders (↑) and removes (×) the rows. |
| [Wallbox](widgets/wallbox.md) | <span class="release-status release-status--changed">Changed</span> | An optional header shows charging power or the site's energy flow; loadpoint discovery fills the site sensors. |
| [Media player](widgets/media_player.md), [Vacuum](widgets/vacuum_tile.md), [Bambu printer](widgets/bambu_printer.md) | <span class="release-status release-status--changed">Changed</span> | Media Play/Pause stays accented while available, including when paused or stopped. Other controls highlight active states or selections; unavailable entities block stale actions, and the printer's Stop is labelled Abort. |
| [Room card](widgets/room_tile.md) | <span class="release-status release-status--changed">Changed</span> | The additional entities are an ordered list of up to seven rows: add, change, reorder (↑) and remove (×) in the Inspector. |
| [Car](widgets/car_widget.md), [Gauge](widgets/gauge.md), [Energy timeline](widgets/energy_timeline.md), [Solar forecast](widgets/solar_forecast.md) | <span class="release-status release-status--changed">Changed</span> | The Inspector shows only configured entity roles; free ones are added through **Add assignment** and removed with ×. Solar forecast days are added in order, each entity once. Car, Gauge and Solar forecast also have layout fixes for larger text and narrow tiles. |
| [Contact](widgets/contact_tile.md) | <span class="release-status release-status--changed">Changed</span> | Layout fixes for larger text and narrow tiles. |

### Fixes

- **Weather and solar:** Weather shows up to 24 hourly forecasts and tap-to-open details from an optional DWD report sensor. Weather and Solar forecast show up to seven days (layouts saved before keep their six and five). Solar's bright bar start is today's remaining part; a tap opens a compact total/remaining popup. Vertical bars are wider on the 4-inch and Tab5 boards.
- **Stocks & ETFs:** quotes come from Easy Stock only. Small quotes keep their precision; the 1D chart covers today from local midnight and stays flat at 0 % when Easy Stock reports no trading.
- **Price forecast:** the 24-hour window starts at the first price when yesterday has dropped out (issue #1); the Inspector restores the saved Today/Tomorrow checkbox.
- **Parqet:** a total sensor with long fund names keeps its holdings, interval and portfolio ids; a sensor in another unit than the total shows **Unit differs**.
- **Car and Vacuum:** preconditioning shows **Sending...** until the climate state changes (60 s limit); a Vacuum press that cannot be sent says so for three seconds.
- **Redraws:** unchanged readings no longer redraw Car, Solar, Stocks, Graph, Price and Wallbox tiles or the Flat, Glass and Matte card styles.
- **Popups and larger text:** dialogs share an opaque themed surface; Room details and history axes fit larger text; light, lock, sensor, car, gauge, to-do, slider and weather tiles keep names on one line and drop elements instead of overlapping.
- **Energy:** daylight-saving days use the full local day; Solar statistics read **Daily production**; the energy nodes follow the card style (Glow halos, Glass translucent, Matte opaque, Flat outlined), also in the editor preview.
- **Wallbox:** unchanged refreshes keep popups open, a missing vehicle shows **Guest vehicle**, and choosing the energy flow fills its site sensors.
- **Entity search:** every widget field shares explicit domain and source rules with matching hints, in creation and in the Inspector.
- **Reliability:** a failed C6 incident write is retried, JC touch scanning starts after the display binding, and large stock layouts keep all history requests.

### Panel and editor

- **Default flip clock:** an empty layout or no visible pages shows centred, animated hours/minutes/seconds with weekday and date, using the system theme, appearance, local time and shared 12/24-hour setting. Restoring a page removes the clock; new installations start with an empty layout.
- **Page visibility:** the page inspector's **Visible on panel** checkbox hides or restores a page and its bottom tab on Save. Hidden pages retain their content and saved graph history, stay editable and are skipped by panel navigation and cycling.
- **Home Assistant connection:** the HA popup shows rejected access and offers **Reconnect**. Camera snapshots use the panel credential.
- **ESP-Hosted 3.0.10:** a C6 on another firmware version is updated once after the first boot of this image, with one extra restart. Settings → Firmware lists the version the C6 reports; Device status hides the C6 memory row when its firmware has no memory monitor.
- **10-inch JC panel:** short taps are no longer lost, and the screen rotates with the PPA again (an ESP-IDF driver bug is fixed in the firmware), freeing 17-34 % of one core.
- **Inspector and preview:** one section order for every widget (Entities, Display, Controls, Placement), entity rows that open the search, explanations behind info icons and device classes next to the entity. Previews follow the panel's text roles and 10-inch layouts and mark overlapping tiles.
- **Entity names:** search, Room details and stock rows share discovered names; a missing `friendly_name` no longer replaces them with the entity ID.
- **Language:** the theme editor, time-zone choices, setup hints, vacuum states and firmware-update progress follow the selected language.

## v0.9.0

Optional [KNXnet/IP Routing](knx.md) is shared by all profiles: default-off editor switch, knxConfig.json edit/import/export, atomic saves, permission-checked objects, cached feedback and backup integration. Xiaozhi exposes configured objects through MCP and preserves tool discovery when microphone capture starts. On audio-capable `panel10`, MCP also advertises capability-gated Common Tools for status, read-only network information (SSID, signal, LAN IPv4, gateway, DNS and MAC), volume, brightness and themes, plus HA discovery/state queries and capability-checked controls restricted to current layout bindings. Controls validate single-entity targets, ranges and choices, reuse the HA service path and distinguish submission from confirmed feedback. Invalid or unallocatable service data cannot become an untargeted request. HA updates flow through the existing cache; layout edits change query/control scope, offline/unknown values are flagged, and fallback deletion events clear stale states. COM24 voice acceptance passed HA WLED power control with feedback and restored color/brightness; other actions have local test coverage. COM24 voice checks passed layout discovery and updated on/off readings; this remains a bounded trial. Annotated User Only Tools provide system information, restart, HTTPS OTA and screen geometry. User tools require an explicit `withUserTools` discovery in the initialized connection; unsupported upstream camera, snapshot, preview and asset tools stay absent. Reference: feat/knx_ip at 61fc9d0b and `betamoojw/xiaozhi-esp32` at `c435f99a`; Tunneling/KNX Secure are excluded. The Waveshare P4 trial verified real WLED power control and restoration; scope and voice-validation results are in [KNX development](development/knx.md). The device-tool adapter is linked explicitly; a static-library regression test covers the missing-tool failure caused by fallback weak symbols.

`panel10` now uses LCD-X BSP 2.0.3, GPIO26 backlight/GPIO27 reset, Rev3.x configuration and a separate dependency lock. COM24 v3.2 display/touch and LAN passed on 2026-10-21; owner confirmed working Live Display. The 32 MiB P4 checksum helper requires a supported flasher stub and rejects ROM fallback; an app-only reflash of `0a551a2` preserved metadata/NVS, coredump and complete LittleFS checksums before boot (see the device guide). CPU rotation avoids intermittent PPA completion timeouts while retaining the display watchdog; COM24 passed a 606-second observation plus owner-confirmed output and touch; all-day stability remains unverified.
Xiaozhi AI adds an off-by-default cloud pairing and push-to-talk path for Waveshare LCD-X `panel10`, with Opus audio, guarded editor controls and a local panel overlay. Its header action uses a robot glyph in the same themed frame as the settings icon. The robot action highlights while its dialogue is visible and restores its idle appearance on close, timeout or disable, including voice-opened dialogues. Optional local WakeNet recognition opens a themed dialogue with a centered emotion and separate recognized-speech/reply text. Voice-opened dialogues use automatic listening and a configurable 10–300 second idle timeout; opening the header icon always shows Hold to talk and waits for a press, even when local wake detection is enabled. Voice-opened dialogues hide Talk while the wake engine is ready, retaining manual fallback on failure. Otto Robot is the default animated emotion style; the original Classic Emoji remains selectable in the web editor and native System Settings. Both use the same saved preference. COM24 passed manual voice, wake/reply, emotion selection and owner-confirmed Otto animation/auto-close on 2026-10-25. The wake wrapper recreates its model to avoid an ESP-SR 2.4.7 P4 cleanup crash and selects MIC1 without mixing the second ADC slot. Its overlay timer is cleaned up during dashboard rebuilds, preventing boot loops after Home Assistant setup even while disabled. A verified local transcript font extends Chinese reply coverage without changing partitions; HTTP socket reservations and bounded single-flight status polling prevent editor connections from consuming the voice/HA budget. COM24 passed 240 bounded HTTP requests and owner-confirmed simplified/traditional replies. Occasional full-screen white flashes remain unresolved, including with Live Display closed; this is not a screen-stability fix. See [resource installation and validation](development/xiaozhi-resources.md), [Xiaozhi AI](xiaozhi-ai.md) and its development reference for the bounded trial.
Native **System settings** (Guition reference `db764156`) adds a rightmost header gear, matching Wi-Fi/HA frames, language, time zone, appearance and colour theme choices, scoped deferred saves, brightness previews and an optional moving clock with timed screen-off. All variants and seven offline languages share the feature. Clock protection also offers an optional analog dial with white hands, a muted coral second hand and shared date, brightness, movement, off and wake controls; existing digital clocks remain the default. Digital time now uses large screen-fitted digits, including portrait and 480-square layouts, while retaining the smaller date and existing time, movement, screen-off and wake settings. All profiles also expose saved 0/90/180/270-degree rotation in the editor and System settings, with independent portrait widget positions, scrolling and retained Tab5 automatic rotation. The clickable yellow Alerts icon precedes the AI action and shows retained crashes, connection failures and configured sensor/doorbell alerts; it hides when empty. The portrait top bar reserves space for the enabled Xiaozhi action alongside connection and settings icons. On 2026-10-24, COM24 Panel10 passed four-angle saves, restart persistence, Live Display and owner-confirmed physical output/touch; other boards have local checks only. See [Display rotation](development/display-rotation.md) and [Top bar](top-bar.md#system-settings). Shared page animations add a 120 ms ease-out slide and 160/120 ms popup and subpage opening/closing fades (settings, connectivity, alerts, widget details/history, options, PIN pad, notifications, camera fullscreen and Xiaozhi), independently implemented for LVGL 9.6 after reviewing SmartPanel `30bcd774`. The saved Page animations switch works in Design and native settings on every profile and survives backup/restore. Rapid navigation and rebuild lifetimes have local checks. The Waveshare LCD-X 10.1 presents complete frames through its existing two DPI buffers to prevent tearing; CPU rotation and bounded completion waits are retained. Frame submission releases the UI without waiting; the next write acquires the released buffer and repairs only changed bounds. In a bounded 2026-10-29 trial at saved 180°, the owner confirmed page slides, settings fades and touch, immediate transitions with animations disabled, and disappearance of the initial horizontal tearing after the synchronized-presentation fix. Other boards have local checks only. The final pre-commit image (`9c8c1b55-dirty`, 2026-10-29 21:57:33) passed owner acceptance of the 160/120 ms Wi-Fi, System settings and Xiaozhi fades: visible, appropriately paced, no tearing and normal touch. Animations were restored off and verified after reload, preserving settings, layout, English, Classic Emoji, 180° rotation and local backups. See [Page animations](appearance.md#page-animations).

### Readable time zones

Adapted from `betamoojw` commit `4c8e40e`: searchable Region/City identifiers, a device-local clock and a bundled recurring-rules catalog shared by all boards. Changes apply immediately; existing POSIX settings and backups stay supported.
The picker and clock status have translations in all seven built-in languages.
### European languages

German retains this fork's complete catalogs. Spanish, French and Polish now
cover the current editor and panel, including complex widgets, Live Display,
settings and diagnostics. The reference is `betamoojw`'s `dev` revision
`db7641566be6b8d36d501154f9c49211ef365367`; its partial catalogs were extended,
without replacing the newer German translations. Native language names appear
in the selector, and custom translations still override built-in strings.
Poppins regular text and emphasis include Polish accents and French ligatures;
the browser preview uses the same expanded subset. Catalog generation is offline.
Host and SDL checks passed. On 2026-10-19, the Guition ESP32-S3-4848S040
also passed German, Spanish, French and Polish rendering checks; the owner
confirmed clear accented characters, stable output and working local touch.

### Chinese and CJK text

Simplified and traditional Chinese are available in the editor and on the panel,
adapted from [`betamoojw` commit `4e117ca`](https://github.com/betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3/commit/4e117ca03c05e7f6ef96de2e435cb24da7c785e7).
The catalogs cover widgets, settings, Live Display, OTA states, theme controls and default titles. Weekday charts use distinct localized abbreviations. JSON sources
generate separate browser modules and portable firmware strings. Shared runtime
TrueType fallback replaces the reference's fixed-size bitmap fonts, preserving
Poppins metrics and supporting Chinese titles and notifications at every text size.
The compact Noto subset covers 802 codepoints, not the complete CJK Unicode repertoire.
It includes every built-in panel translation and common room/device names without
consuming the S3's PSRAM headroom with a full CJK font.
See [Language](appearance.md#language) and the font source guide in `main/ui/fonts/README.md`.

### Browser live display

Live LCD mirroring and remote touch are adapted from [`betamoojw` commit `57df6a3`](https://github.com/betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3/commit/57df6a3d91defebb10bd361142e53f2619346470).
The shared implementation supports all board geometries, partial/direct/full
refresh and runtime rotation rather than the reference's fixed 480 × 480
full-refresh profile. The editor adds Live Display, optional remote control,
actual-size view and PNG download. Authentication, same-origin session tokens,
physical-touch priority, input cancellation and OTA suspension are retained.
Capture replays only dirty regions after the initial frame, avoiding repeated
full-screen redraws for small changes. The COM18 S3 unit passed browser mirror,
remote navigation, reconnect and settings-preservation checks on 2026-10-19;
the owner confirmed stable colors/position with the mirror both open and closed
after the direct-EDMA change. Other boards have host coverage, not device proof.
See [Web editor](editor.md#live-display) for use and limits.

### Local S3 support

This checkout restores the Guition ESP32-S3-4848S040 (`panels3`) profile removed
by the original v0.9.0 fork. It uses ST7701S RGB, GT911 touch, native Wi-Fi, 16 MB
flash, 8 MB Octal PSRAM, five pages and a compact font profile. The shared display
adapter, brightness policy and S3 OTA identity are retained. P4-only dependencies
and C6 updater code are excluded from S3 builds. A Windows PowerShell build wrapper
is available. See the [device guide](hardware/panels3.md) for limits and validation;
the connected S3 boots, its owner confirmed clear text/colors, accurate touch and
normal page switching after changing RGB to partial refresh, and the editor/API
respond over native Wi-Fi.
Other hardware checks remain pending and no new release is published.
Complex S3 cards now have their own calibrated text-growth table. The Energy
page fits all six nodes on 480 × 360, Wallbox fits from 320 × 320, and date/time
fields scroll beneath fixed actions. Graph statistics no longer overlap the plot;
translated weather/detail rows and date/time actions fit their available width.
Solar detail text follows the theme and all six forecast rows fit the popup.
The S3 RGB profile uses 6 MHz direct EDMA and internal partial draws.
Serialized writes schedule one resynchronization on LVGL after a quiet interval.
Boot-overlay hide retries through LVGL after lock timeout. The owner confirmed
stable idle, saves/page changes and aligned touch on the COM18 S3.

!!! info "Development fork · based on upstream v0.8.2"

    Changes below belong to **andre68723's fork**, compared with upstream commit
    `420df764`. Images for all five variants are on
    [GitHub Releases](https://github.com/andre68723/BETTA-HA-PANEL/releases).
    `paneljc-v2` remains compile-checked only; the local LCD-X panel10 trial is documented above. **Guition JC and Tab5 support are new in this fork**; they have no upstream v0.8.2
    baseline. Device measurements below validate fork builds, rather than compare
    them with upstream firmware.

### At a glance

| Area | What changes in this fork |
| --- | --- |
| [Toolchain](release-notes-architecture.md#toolchain-and-dependencies) | **ESP-IDF 5.5.2 → 6.1**, **LVGL 9.5.0 → 9.6.0**, updated and pinned shared dependencies |
| [Display devices](choose-display.md) | Guition 10.1-inch and M5Stack Tab5 profiles join the Waveshare P4 devices; hardware validation is tracked per device |
| [Widgets](#widgets-new-and-changed) | **19 new types**, **11 updated types**, plus the existing Empty spacer |
| [Appearance](appearance.md) | Flat, Glow, Glass and Matte, independent color themes, larger text and consistent icons |
| [Pages and editor](editor.md) | Room/house imports, searchable entity picker, history options, live settings and more pages |
| [Notifications](notifications.md) | MQTT/HTTP alerts, camera doorbells, Home Assistant triggers and optional sound |
| [Updates and recovery](#updates-and-rollback) | Bootable OTA slots, rollback, retained crash evidence and optional automatic updates |
| [Documentation](documentation.md) | No-code setup through the first light, graphical MQTT notifications, focused widget guides and a searchable Zensical site; exhaustive style comparisons under Development |

### Known limits

!!! note "Device validation and remaining release gaps"

    - Fork development stress runs with two editors (`tools/stress_test.py
      --webui-copies 2`) passed on panel4 and paneljc without failed requests.
      Earlier fork development freezes and asset-loading failures are documented
      with their test dates in [Development](development/learnings.md).
    - The new flash layout is verified on panel4 and paneljc only. Graph history has a quota (a quarter of the filesystem, at most 1 MiB, plus a 512 KiB free reserve) and removes orphaned files; the quota is host-tested, but its effect on the 16 MiB boards (2.625 MiB filesystem; a full graph file is about 32 KiB) is not measured on a device, and panic dumps were
      measured idle (about 21-23 KiB), not under load.
    - Panel10 passed a Rev3.2 device trial; paneljc-v2 is compile-checked only. Tab5 ran on one device
      (ST7121, ESP32-P4 v1.3; first trial 2026-10-05). Panel4, paneljc and Tab5 were
      updated over the air to v0.9.0 on 2026-10-15. Panel10 targets v3.x; other P4 profiles target v1.x
      silicon. See [hardware](choose-display.md).
    - The Alarm tile was tried against a template alarm with and without a numeric
      code (2026-10-15); a wrong code and text codes have not been tried.
    - Some interactions still need device trials, including doorbell camera and
      unlock, factory reset/restore, car pre-conditioning and fan controls.

### Upgrading from upstream v0.8.2

!!! warning "Older flash layouts need one USB migration"

    OTA updates cannot install the new partition table or rollback bootloader.
    Export a full backup before migrating. A combined factory image erases the
    credential region; moving LittleFS also requires copying its files to preserve them.

| Starting point | Installation path |
| --- | --- |
| Firmware older than upstream v0.8.2 | Update to v0.8.2 first; older settings formats are no longer read |
| Waveshare on upstream v0.8.2 | USB flash with the matching new table/bootloader; factory images start at `0x0` |
| First install on Guition or Tab5 | Use the matching factory build; these profiles have no upstream v0.8.2 image |
| BETTA with the current table and bootloader | Matching app updates can use OTA |

Follow [Installation](getting-started.md) and [partition migration](development/build.md#changing-the-partition-table).

??? info "Device-specific migration notes and validation"

    - **Upgrade path:** update older upstream firmware to v0.8.2 first. This fork
      no longer reads settings formats from before v0.8.2.

    - **One-time panel4 USB flash:** a panel4 coming from v0.8.2 needs the new
      bootloader and partition table over USB. A combined factory image starts
      at offset `0x0`. OTA cannot replace the old table, whose second app slot is
      above the ESP32-P4 bootable range and which has no crash-dump partition.
      A factory flash erases credentials; the current table also moves LittleFS.
      Use the documented migration to preserve NVS and copy the filesystem, or
      restore a backup and enter Wi-Fi/Home Assistant credentials again.

    - **JC8012P4A1 first install:** there is no upstream v0.8.2 image for this
      board; use a factory image. Subsequent app updates use OTA.

    - **M5Stack Tab5 first install:** build the `tab5` variant and use its factory
      image; there is no inherited upstream image for this board. The firmware
      ran on one ST7121/ESP32-P4 v1.3 unit on 2026-10-05. Follow the remaining checks
      in the [Tab5 display device guide](hardware/tab5.md#validation-status-and-next-checks).

    - **Flash layout, USB update required:** every ESP32-P4 board uses two 6.5 MiB OTA slots, a 256 KiB coredump partition and
      LittleFS from 0xD60000 (2.625 MiB on 16 MiB devices, 18.625 MiB on 32 MiB devices).
      JC and Tab5 had no upstream partition table. NVS keeps its 24 KiB and address. A panel on another table, for example upstream v0.8.2, needs one USB flash with the new table
      (OTA cannot change it: an OTA update runs fine but leaves the old table and filesystem in place) and the filesystem moves, so its layout, settings, themes and graph history are formatted away unless
      copied over (`tools/lfs_repack.py`, see development.md "Changing the partition table"); a flash that skips the NVS range
      keeps the stored passwords and token, the combined `*.factory.bin` erases them. Hardware-checked on panel4 and paneljc
      using the NVS-preserving migration and filesystem copy; both retained their
      layout, settings and Home Assistant connection. On the panel4 and the paneljc an OTA upload into each slot was
      accepted and validated after 60 s (ota_0 to ota_1 and back), and a test image that panicked while pending verification was rolled back to the
      previous app by the bootloader, with a decodable panic dump left in the new coredump partition (20.6 KiB on the panel4, 22.4 KiB on the paneljc).
      `panel10` passed the documented LCD-X trial; `paneljc-v2` is compile-checked only; one Tab5 device trial began on 2026-10-05.

### Hardware and platform

- **Display device support:** upstream's Waveshare `panel4` and `panel10` profiles
  are retained. **Guition JC8012P4A1 (`paneljc` / `paneljc-v2`) and M5Stack Tab5
  (`tab5`) are added by this fork.** Panel4 and the original paneljc have device
  trials, including one Tab5 on 2026-10-05; panel10 has a Rev3.2 trial and paneljc-v2 is compile checked only. Builds use
  ESP-IDF 6.1 and matching ESP-Hosted/C6 firmware.

- **M5Stack Tab5:** 5-inch ESP32-P4 + ESP32-C6, with landscape and portrait
  layouts. BMI270 automatic rotation defaults on and can be disabled in Display
  settings. A one-second stable hold selects one of four orientations; flat or
  shaking positions retain the current view. The boot screen reads the mounting
  orientation before it appears, with the last stable view as fallback when the
  sensor is unavailable or the panel lies flat. Stable applied orientations are
  remembered without changing the fixed landscape mode. Portrait widgets can be positioned
  independently and long pages scroll. The editor refuses saves from a stale
  orientation. Solar forecast cards can use the full 720-pixel portrait width.
  Other boards retain fixed orientation. Open popups and display
  protection defer rotation. One ST7121/ESP32-P4 v1.3 unit ran the landscape
  dashboard, camera-brightness and quiet speaker tests on 2026-10-05; camera
  sensing still defaults off. New installations use 60% chime volume and enable
  night mode from 22:00 to 06:00 at 1% brightness; saved settings retain their
  values. Sound remains opt-in. See the [Tab5 guide](hardware/tab5.md) and
  [display settings](display.md#tab5-automatic-rotation).

- **Cover callbacks after dashboard rebuilds:** cancelling a media or camera
  widget invalidates every outstanding image request through final dispatch.
  Reusing a widget's heap address cannot revive a deleted consumer's callback.
  This fixes the double-free exposed by the first Tab5 rotation device trial.

- **Connectivity and touch:** Wi-Fi roaming and serialized C6 calls improve
  connection reliability; panel4 touch probes both GT911 I2C addresses.

??? info "Touch support for the new JC profiles"

    The fork's JC support uses Espressif's Apache-2.0 `esp_lcd_touch_gsl3680`
    driver with raw coordinates and host-tested nearest-point tracking in
    `drivers/touch_track.c`. Stray or reordered reports are held and logged as
    `touch jump held`. There is no two-finger zoom tracking. The current driver
    revision is compile checked; its orientation and touch behavior still need
    checking on a paneljc unit.

- **Display settings:** one page with headings, and one compact reset at the bottom
  that restores every Display group and the board-specific camera and rotation
  defaults. Sound and other settings pages stay unchanged.
    - New installations enable night mode (22:00–06:00, 1 %) on every board; saved
      preferences are kept.
    - Disabled night and presence features hide their controls but keep saved values.
      Sensitivity, camera testing and pixel movement use compact expandable options.
    - Use, inactivity and night brightness each have a 30-second test with live slider
      changes, a device-side timeout and Stop. A test leaves the night schedule alone,
      touch cancels it, and a wake touch cannot activate an underlying widget.
      Requests retry briefly while the display redraws, keeping slider and Stop in order.
    - Tab5 keeps the BSP's 5 kHz, 10-bit backlight PWM and logs duty, range and
      frequency readback. Known limit on the owner's ST7121 unit (2026-10-13):
      1–50 % showed little visible difference, 75 % was brighter and 0 % switched
      off. Alternative PWM profiles did not change this; further trials were stopped.
    - Tab5 uses Espressif BSP 1.3.2 with the display-initialization patch for a NULL
      configuration. Optional backlight diagnostics distinguish LEDC register values
      from the digital signal sampled at GPIO22.

- **Shared display integration:** Waveshare LCD-4B uses BSP 3.0.1 with a reproducible adapter-constraint override.
  Every board uses the shared `esp_lvgl_adapter` 0.7.3 runtime for display, touch and locking, with PSRAM partial
  buffers, runtime/boot rotation and the bounded PPA watchdog preserved. Original JC continues CPU rotation;
  Tab5 keeps BSP 1.3.2 and its unchanged backlight defaults. The 4-inch profile retains GPIO23 touch reset and
  both GT911 addresses. Some BSPs still pull in the old port as an unused runtime compatibility dependency.
  On 2026-10-13, source `650fd258` passed OTA/API checks on the local 4-inch, original JC and ST7121 Tab5;
  the owner confirmed physical orientation and touch navigation on all three (Tab5 mounted at 270 degrees).

- **Scheduled night dimming/off:** configure a local-time window, including
  across midnight, under **Settings → Display**. A night brightness of **0%**
  switches the backlight off; higher values dim it. Touch wakes the display
  for the normal dim timeout, then the night level returns. At the end of the
  window, normal brightness resumes. Camera presence wake during this window
  requires **Wake at night**. See [Display and brightness](display.md).

- **Display and presence:** runtime controls cover active and dimmed brightness,
  plus optional display protection that moves weather or cycles pages before
  switching the backlight off. JC, Tab5 and Waveshare LCD-X (`panel10`, OV5647) cameras sense light/movement without recording or streaming in automation mode. LCD-X adds [default-off JPEG/MJPEG output](cameras.md), timed preview, software image adjustment, four motion zones with threshold/area/duration/cooldown controls, and four saved HA/IP JPEG sources usable in tiles and full-screen views. Disabling output clears old pictures. Settings persist separately; backups omit passwords. Adapted from Guition `7668ebe6`, with native/half-size OV5647 output that follows display rotation, swaps dimensions on quarter turns and keeps saved motion areas anchored. Local checks and the panel10 build pass; a bounded LCD-X trial passed JPEG/MJPEG, image controls, timed tests and four-angle output decoding/browser preview, with owner-confirmed physical orientation. Native snapshot/live tiles and full-screen touch passed at display setting 90, including page pause/resume and restoration. Motion-zone coexistence and external-source acceptance remain pending. [Feature mapping](development/local-camera.md) records ISP and sensor differences.
  LCD-X defaults off and supports brightness adaptation, approach/night wake, sensitivity 1–10 (default 5) and a device-timed 30-second test.
  Actual exposure/gain compensate sensor auto exposure; failures invalidate motion. LCD-X light/approach/night/30s checks passed a bounded 2026-10-27 trial; wider calibration remains pending. CSI shares the display's interrupt core.

- **Sound:** optional generated notification and doorbell sounds on panel4 and
  Tab5 have volume, night mute and a test control. JC audio is disabled in current builds. Existing notifications
  stay silent until sound is enabled. Panel4 completed 50 API-triggered sound
  tests without audio/I2C errors or a falling heap; acoustic quality, touch
  coexistence and broader acoustic checks remain pending. The owner heard a quiet Tab5 test chime on 2026-10-05.

- **Capacity and diagnostics:** ESP32-P4 boards cache up to 4096 Home Assistant
  entities. The editor and firmware expose board, component, memory, restart
  and network diagnostics;
  the optional betta animation can be disabled to save flash.

### Widgets: new and changed

**New** means a type added since upstream v0.8.2; **Changed** means an existing
upstream type was updated. Empty spacer keeps its original role. Shared appearance,
font and icon changes apply across the visible widgets.

| Widget | Status | What is new or changed |
| --- | --- | --- |
| [Alarm](widgets/alarm_tile.md) | <span class="release-status release-status--new">New</span> | `alarm_control_panel` state with arm/disarm from a tap, only the modes the entity supports, and a numeric code pad when it has a `code_format`. The code is never stored. Text codes are not supported yet. |
| [Bambu printer](widgets/bambu_printer.md) | <span class="release-status release-status--new">New</span> | Print status, nozzle and bed temperature, progress and remaining time of a Bambu Lab printer through the Home Assistant integration, with Pause/Resume, a read-only Status popup and Stop after a confirmation. Temperatures sit below the illustration, or beside it in wide cards, without covering the drawing. The editor finds the printer's entities by choosing the printer (any Home Assistant language); they are saved as explicit bindings. Simulator and host tested; the printer lookup is verified on the paneljc against the owner's Home Assistant, Pause/Resume/Stop not yet against a live job.  Optional camera binding adds **Status → Live camera** using the shared MJPEG view; live camera device validation remains open. |
| [Binary sensor](widgets/binary_tile.md) | <span class="release-status release-status--new">New</span> | Readable binary/event states and optional Recorder history on tap. |
| [Button](widgets/button.md) | <span class="release-status release-status--changed">Changed</span> | Shared appearance styles, readable labels and touch targets. |
| [Camera](widgets/camera_tile.md) | <span class="release-status release-status--new">New</span> | Snapshots plus opt-in live picture, full-screen view on tap, hidden/sleep pause and bounded buffers. Sources include HA, LCD-X built-in and saved IP/HA cameras; no RTSP/H.264/H.265 or audio. The LCD-X built-in tile/full-screen trial passed; external-source acceptance remains open. |
| [Car](widgets/car_widget.md) | <span class="release-status release-status--new">New</span> | Battery, range and charging; optional pre-conditioning, lock and interior temperature. |
| [Climate](widgets/climate_tile.md) | <span class="release-status release-status--new">New</span> | Air-conditioning controls with supported HVAC, fan and swing modes. |
| [Contact](widgets/contact_tile.md) | <span class="release-status release-status--new">New</span> | Door/window views, readable state words and optional history. |
| [Cover](widgets/cover_tile.md) | <span class="release-status release-status--new">New</span> | Open, stop, close and position controls. |
| [Date/Time](widgets/datetime_tile.md) | <span class="release-status release-status--new">New</span> | Edit supported HA date/time entities in local time; invalid daylight-saving times are rejected. |
| [Empty spacer](widgets/empty_tile.md) | <span class="release-status release-status--unchanged">Unchanged</span> | Retains its layout-spacing role; no entity or action. |
| [Energy timeline](widgets/energy_timeline.md) | <span class="release-status release-status--new">New</span> | 24-hour flows, source shares, rings and a detail popup, independent of an Energy page. |
| [Fan](widgets/fan_tile.md) | <span class="release-status release-status--new">New</span> | Speed, oscillation and presets, with a larger rotor view. |
| [Gauge](widgets/gauge.md) | <span class="release-status release-status--new">New</span> | Linear or radial readings with units, HA display precision and optional extra values. |
| [Graph](widgets/graph.md) | <span class="release-status release-status--changed">Changed</span> | Local sampling or optional 24-hour Recorder history, with updated line/smoothed/bar presentation. |
| [Heating](widgets/heating_tile.md) | <span class="release-status release-status--changed">Changed</span> | Improved controls, state handling and compact/large layouts. |
| [Light](widgets/light_tile.md) | <span class="release-status release-status--changed">Changed</span> | Style-aware controls, state handling, an effect picker where supported and wider Matte layouts. Colour temperature is Kelvin only (Home Assistant 2026.3 removed mireds). |
| [Lock](widgets/lock_tile.md) | <span class="release-status release-status--new">New</span> | Lock/unlock/open where supported, optional confirmation and history on long press. |
| [Media player](widgets/media_player.md) | <span class="release-status release-status--changed">Changed</span> | Larger volume controls, shuffle/repeat where supported, radio layout and cover-art handling. |
| [Room card](widgets/room_tile.md) | <span class="release-status release-status--new">New</span> | HA area status/actions, plus an optional humidity, cover or media field. |
| [Scene](widgets/scene_tile.md) | <span class="release-status release-status--new">New</span> | Dedicated tile for scenes, scripts and momentary HA actions. |
| [Select](widgets/select_tile.md) | <span class="release-status release-status--new">New</span> | Choose a select/input-select option through its popup. |
| [Sensor](widgets/sensor.md) | <span class="release-status release-status--changed">Changed</span> | HA display precision, optional name/age and Recorder history. |
| [Slider](widgets/slider.md) | <span class="release-status release-status--changed">Changed</span> | Improved fitting, controls and wider Matte presentation. |
| [Solar forecast](widgets/solar_forecast.md) | <span class="release-status release-status--new">New</span> | Daily yield with optional weather-date alignment; today shows the weekday and its total, with the remaining part bright; editor bars keep their full height. |
| [Sonos favorites](widgets/sonos_favorites.md) | <span class="release-status release-status--new">New</span> | Play configured media favorites directly. |
| [Todo list](widgets/todo_list.md) | <span class="release-status release-status--changed">Changed</span> | Improved state handling, controls and layout. |
| [Vacuum](widgets/vacuum_tile.md) | <span class="release-status release-status--changed">Changed</span> | General vacuum tile replaces the Roborock-specific type and also serves Matter-based vacuums; legacy layout name remains compatible. A suction-level picker follows the vacuum's supported fan speeds. The Locate button is gone, the charge comes from a battery sensor (Home Assistant 2026.9 removed the vacuum's own battery value; the editor suggests the sensor of a new card), and with **History on tap** a long press opens the last 24 hours of activity. |
| [Wallbox](widgets/wallbox.md) | <span class="release-status release-status--new">New</span> | Minimal evcc loadpoint control through Home Assistant: direct Off, Smart/Fast mode, vehicle assignment and target SOC with Apply, plus status, power, charged energy, remaining time and a solid green horizontal SOC bar. Sessions lists completed charges; without a cable, the modes remain visible and the last session replaces SOC/target. The editor detects loadpoint bindings without a full registry dump and prefers the number charge limit. Commands wait for HA state confirmation; physical charger acceptance remains open. |
| [Weather (now)](widgets/weather_tile.md) | <span class="release-status release-status--changed">Changed</span> | Shared condition icons, extra readings and optional Show name. |
| [Weather forecast](widgets/weather_3day.md) | <span class="release-status release-status--changed">Changed</span> | Available highs/lows/rain, daily report details, compact fitting and optional Show name. |

See [Integrations](home-assistant.md) for entity requirements and [Widget anatomy](widget-anatomy.md) for sizing and touch guidance.

### Special page types

- **Room overview:** import Home Assistant areas in one step. Entity discovery inherits areas from parent devices and follows HA display names, including empty entity-name overrides. The page groups
  their room cards by floor and updates them from the area's entities. Room
  cards can show an extra humidity, cover or media field with a direct action
  where space allows.
- **Room page:** choose one Home Assistant area, then select which temperature,
  humidity, presence, door/window, climate, media, light, cover, fan and vacuum
  entities to include. A live preview shows the proposed page: readings at the
  top, climate and media on the left, lights and covers on the right. The
  result stays freely editable in the normal layout editor.
- **Energy dashboard:** the existing page gains clearer node values and units,
  style-aware circles and bidirectional grid/battery readings on panel4. The
  independent Energy timeline is a normal widget and needs no Energy page.
- **Page management:** pages have icons, can be reordered and duplicated, and
  duplicated widgets receive fresh IDs. The limit is 12 pages on 1280 px
  boards and 8 on the 4-inch panel (compact bottom tabs, see "More pages" below).
  Bambu, EVCC and Sonos Favorites cards keep their current readings when switching
  pages, avoiding redundant state replay and list rebuilding before the first frame.

- **More pages:** upstream v0.8.2 allows **5 pages on every variant**. The fork
  allows **8 on panel4** and **12 on 1280-pixel profiles**; the 12-page limit also
  applies to the newly added JC and Tab5 profiles. Tabs become compact when full
  labels no longer fit: the active page keeps its title, others show an icon or
  number. The editor warns about duplicate icons in the compact range.

- **Larger layouts:** the JSON limit rises from upstream's **16 KB to 32 KB**.
  Current profiles support up to **224 widgets**, with the editor reading the
  device's limits. These limits also define the new JC and Tab5 profiles.

### History and energy timeline

- **Optional history:** sensor, binary sensor and contact tiles open Recorder
  history on tap; locks use a long press to preserve their tap action. Numeric
  entities show a 24-hour chart and discrete entities a state timeline.
  Its Inspector checkbox is applied immediately and retained on save/reload.
  History loads on demand, handles unavailable periods and shortens oversized
  replies. It is off by default and requires Recorder data. A numeric
  `measurement` sensor with long-term statistics also offers **7 days**: 168
  hourly means with real gaps (tried on a panel4, 2026-10-07).
- **Seven-day Energy details:** Gas, Water and Solar open seven days of daily
  consumption, in a popup or a right-hand panel (tried on a 10-inch panel).
- **Graph data sources:** graphs retain local sampling by default. The new
  **History from Home Assistant** option fills a 24-hour graph from Recorder,
  including after a restart, without storing those samples on the panel. Up to
  eight graphs use this source, fetched sequentially and refreshed every
  15 minutes while their page is shown. The Inspector checkbox is applied
  immediately and retained on save/reload. Repeated refreshes keep Recorder
  samples separate from live values, preventing the loaded history from disappearing.
  Empty refreshes retain the last usable graph. See [history sources](history.md#choose-the-history-view).
- **Energy timeline:** Home Assistant long-term statistics supply 96
  quarter-hour bars for solar, grid and optional battery flows. Labeled rings,
  an optional Solcast forecast, a details popup and a nested-rings look show
  source shares; missing intervals remain gaps. Allocations inferred from
  aggregate counters are marked as estimates, while measured counters remain
  available in details. Visual inspiration:
  [aharren/ios-energy-widget](https://github.com/aharren/ios-energy-widget)
  (MIT); the implementation is new.

- **Camera tile:** a picture request that gets lost no longer leaves the tile on its placeholder; it asks again after 30 seconds.

### Shared widget improvements

??? info "Rendering, controls, precision and image handling"

    - **Weather:** updated upstream weather tiles use shared condition icons,
      available high/low/rain readings, optional names and daily report details.
      The new Solar forecast can align its dates with weather while keeping
      remaining yield separate.
    - **Values and units:** sensor and gauge readings use Home Assistant display
      precision when provided. Solar, car and gauge values include units;
      sufficiently large distance displays can convert metres to kilometers.
    - **State handling:** shared HA attribute compaction handles large payloads
      for existing and newly added widget types. Existing Vacuum, Light,
      Heating, Media and Todo widgets have updated controls and layout handling.
    - **Images:** cover art and the new Camera tile use area-average downscaling.
      Hardware-decoded JPEGs are reduced before display, and decoder padding is
      cropped. Device checks with real covers remain pending for this decoder path.
    - **Navigation and media controls:** bottom tabs have equal widths when the
      first title fits. Volume buttons, slider and touch targets have larger
      layouts for the retained Waveshare profiles; the new JC/Tab5 profiles use
      those shared controls. Current dimensions are 46 px buttons / 20 px track
      on 1280-pixel-wide 10-inch displays, and 52 / 22 px on panel4. Supported
      players expose shuffle/repeat; radio streams omit duration-dependent rows.
      Taller cards distribute space around the cover.
    - **History:** the newly added Recorder popup uses round value-axis ticks,
      units, grid lines and Min/Max with precision suited to the tick interval.
    - **Editor previews:** the new Solar forecast uses full-height bars.
      Selecting an existing page/widget keeps the saved/unsaved state unchanged.

### Appearance styles

Appearance is a new setting independent of the color theme. The four styles
apply to widgets, controls, energy nodes and the editor's layout preview:

| Style | Look |
| --- | --- |
| Flat | Crisp outlines and near-square cards/tracks, without depth effects |
| Glow | Rounded, raised cards and inset tracks with soft shadows |
| Glass | Translucent, frosted cards with a light edge and distinct page background |
| Matte | Opaque, muted surfaces, active-state colors and wider Light/Climate/Slider layouts |

The styles work with six built-in color themes, including the new **Pure Black**,
and custom themes. Value-coded fill gradients keep their meaning in every
style; text and symbols use the same fonts throughout. See
[appearance](appearance.md) and the four-style [widget gallery](gallery.md).

### Typography and readability

- **Fonts and icons:** all text, symbols, icons and emoji are drawn at run time by `lv_tiny_ttf` from four small embedded subsets (Poppins Medium, Poppins Bold for notification emphasis, Material Design Icons,
  Noto Emoji) instead of generated bitmap fonts: about 120 KB of font data (15 + 26 + 81 KB) replace roughly 490 KB of compiled bitmap fonts, and any size is available. Material Design Icons
  replace mixed built-in UI symbols; accents, supported units and emoji render consistently across widgets, popups and the boot screen. `lv_font_conv` is no longer used;
  `tools/generate_widget_fonts.py` subsets the sources with fontTools.

- **Text size setting:** Text size in the Layout header (Standard, Larger, Largest) applies and saves at once without a restart. Text asks for a role (small notes, names, values,
  prominent and large readings); the size follows the role, the board (the Tab5 starts one step above the Panel4, the 10-inch boards use the Panel4's sizes) and the user's step.
  A tile too small for larger text keeps a smaller one (calibrated from the layout fixtures), and `scripts/check.sh` renders every variant at all three steps.

- **Notification emoji:** notification titles, messages and action labels show supported monochrome emoji (about 180 faces, reactions, weather, alerts, device states and arrows);
  joined emoji, flags, keycaps and unsupported emoji appear as `[Emoji]`, and the text always fits its buffer without splitting a character or an emoji sequence (a text that is too long is cut before the whole emoji). The notification card draws a selected set of 37 emoji in color (list in [Notifications](notifications.md#emoji)); every other emoji stays monochrome.

- **Subscript digits:** the shared text font includes `₀`–`₉`, so notifications and readings show chemical names such as `CO₂` and `PM₂.₅` correctly. Superscript units `m²` and `m³` remain supported.

- **Top-bar stock quote:** this fork adds the stock slot. On panel4 and Tab5 it
  keeps 20 px text, shortening the exchange suffix, currency and then symbol
  when space is tight. Other profiles can use a 16 px fallback.

??? info "Panel4 readability checks"

    Larger labels and values fit compact weather layouts, with condition icons
    moved into the day popup where needed. The maintained panel4 layout passed
    a 90-second navigation/discovery/editor workload without a reset or coredump.
    This is a functional check of a fork build; physical readability at 30 cm
    still needs assessment. See the [4-inch simulator example](pages.md#free-pages).

- **Top bar status chips:** Wi-Fi, Home Assistant and MQTT each have their own compact chip, and each opens its own detail card (Home Assistant with the firmware version, MQTT with broker and device id). The MQTT chip appears only while MQTT is running. On the Tab5 the touch area of each chip fills the whole 60 px bar.
  Detail cards grow for larger text, wrap long values and scroll when necessary, keeping Wi-Fi and firmware information readable at Largest.

- **Top bar items:** the 1280 px boards (10-inch and Tab5) show up to three items left of the clock (date, weather, stock quote); the 4-inch panel shows two. The third item is set in Settings > Top bar.

- **Top bar and navigation:** date, weather and stock slots sit beside live
  Wi-Fi/Home Assistant/MQTT status popups. The stock quote fits beside the date
  and clock on panel4; the Wi-Fi icon conveys connection and signal strength.
  Navigation follows editor page order, shortens long labels and uses larger
  popup close targets.

- **Layout fitting:** important readings and controls stay inside short or
  narrow cards, including weather, heating, sensor, gauge, car, slider, cover,
  media and climate. Long labels truncate, unavailable values are formatted,
  and value-coded buttons keep contrast across styles. The SDL
  [widget gallery](gallery.md) and [simulator](simulator.md) cover these layouts.
- **German and English texts:** every built-in section follows the panel language.

- **Notification emphasis:** message bodies recognize paired `**bold**` and `__bold__` markers, including inline
  text, using a Poppins Bold subset with the same line metrics and Unicode/emoji support. Plain messages keep
  their existing label rendering. A bounded parser preserves unmatched/escaped markers and falls back to literal
  text at the 32-run limit; replacement, scrolling, tap dismissal and last-message recall retain their behavior.

### Web editor

- **Settings layout:** room and Home Assistant name inputs sit together; the
  firmware build timestamp appears once in the editor header. Network settings
  omit the redundant setup access-point instructions already shown during setup.
  Home Assistant setup offers a button to insert the suggested WebSocket URL
  into the editable field.
  Display settings group brightness, protection timings and night scheduling
  into compact rows that wrap on narrow screens.
  Alert settings use compact columns and include an offline gallery example
  of the camera/action notification card.
  Reloading keeps the current editor menu in the same browser tab. Clearing a
  crash report hides its actions, and status refreshes retain its timestamp.
  Firmware has its own menu; admin password controls are grouped with Backup & security.
  Language & time has a separate System page with collapsible custom translation
  upload. Menu labels expose MQTT and security; appearance and colour theme stay
  together before the top bar. Field widths match the content they hold.

- **Finding and adding widgets:** one searchable dialog combines widget types
  with compatible Home Assistant entities. The page menu groups blank, room,
  house and energy pages; an empty page offers immediate add actions.

- **Editing layouts:** the existing drag/resize editor gains a page inspector,
  room import, keyboard controls, reorder, widget duplication and list search
  for larger pages. A duplicated widget keeps its settings; a duplicated page gets fresh widget IDs.
  The widget row menu copies or moves a complete widget to another normal page, preserving every entity
  binding and option, checking widget/JSON limits and selecting its destination.
  The compact header groups title, status and Text size, with translated Reload/Save tooltips and a JSON import/export menu beside dark/light. Font-size details open on demand; narrow screens wrap the header, leaving the right column for the inspector.

- **Energy timeline colors:** Appearance → Colors offers six flow colors with swatches, HEX editing
  and reset to the original palette. Saved overrides apply to the tile, detail popup and editor preview.
- **Firmware settings:** board and RAM information appear once in Device status. Manual update checks
  send JSON, matching the API request guard and avoiding HTTP 415.
- **Inspector and previews:** previews show current entity values and units,
  the selected theme/style and Poppins text. A shared **Show name** option
  frees space on named tiles while old sensor settings remain compatible.
  History, forecast day count, solar alignment and gauge sizing have their
  own controls and hints.
  Properties and Position & size use separate tabs. Compact field labels, wrapping entity IDs and
  magnifying-glass Search buttons keep bindings readable. The Energy timeline folds optional battery
  and forecast bindings; Appearance opens automatically when it fits. Extended Inspector explanations
  use a short label and an info icon opening the complete text in a popup. Desktop page/widget lists
  scroll independently with matching row widths.
  Page title/navigation and optional Energy-page sensor groups open separately from the main source and view options.
  The fan preview uses three curved blades and a central hub for a clearer fan silhouette.
  Sample texts in the previews follow the editor language (several were
  German-only) and take state from the entity where there is one (contact,
  vacuum). The media player preview hides the cover and the heating preview
  shows target and status like the panel, so neither overflows or differs
  at the gallery sizes.
  The gauge looks (smooth, segmented, radial), graph modes, semi-arc heating, forecast weekdays and
  precipitation, the todo count and the energy timeline's rings (also nested) now follow the panel too.
  Previews scale with the tile (text, arcs, icons, switches) and drop what the panel drops at small sizes;
  sliders show their value in the bar, the button its state and a large switch, and contact, scene, lock and binary tiles use the panel's icons.
  The house page preview uses the panel's grid and room cards with the states of the room's entities.
  Editor usability: Settings > Design and Top bar are separate pages; long settings hints show their first sentence with an info button;
  the right column stays reserved for the selected widget or page; below 1100 px the
  canvas rises (lists collapsed, page chips, sticky header actions, a multi-column settings navigation);
  the display settings line up on one grid; the delete buttons keep their distance from the duplicate buttons.

- **Safer changes:** unsaved edits mark Save and trigger a warning before
  Reload or leaving. Toolbar Undo/Redo and Ctrl/Cmd+Z restore layout edits, including page changes,
  geometry, Inspector fields and imports. A completed drag or continuous field edit is one step;
  history retains up to 50 steps within a 2 MiB UTF-8 budget. The deletion toast uses the same history.
  Save updates the dirty baseline without clearing history; edits during a pending save remain unsaved.
  Failed reloads preserve model, selection and history, and show an error rather than substituting defaults.
  A successful reload resets history; late responses cannot replace intervening edits.
  Imports report invalid structure, excessive size or off-canvas widgets;
  editor size limits match firmware. Saving keeps the current page selected. The badge compares the layout with the loaded or saved one, so selecting a page or a widget no longer counts as an edit.

- **Entity search:** every widget entity field has a Search button, including the
  Energy timeline's battery and forecast sensors, vehicle controls, vacuum battery,
  Bambu bindings, Room entities and advanced Wallbox bindings. Results respect
  each field's allowed domains; Room extras remain limited to the card's entities.
  Manual Energy-page sensors also offer Search.
- **Inspector consistency:** the new Gauge keeps extra entities through import and Save; sensor name
  and freshness switches apply immediately. The missing-entity banner clears
  after Save, inspector hints fit narrower windows, and translated picker and
  page controls no longer expose raw translation keys. Sidebar controls have
  clearer accessibility labels.

- **Settings and backup:** settings are grouped by task and apply live where
  possible. Save restarts only when Wi-Fi, Home Assistant, MQTT, time server or
  room settings change. Backup/restore includes layout and customization
  without secrets, also the text size and the update options; a test compares
  the settings the panel reports with what the backup writes.

### Notifications and MQTT

- Notifications arrive through the HTTP API or MQTT and can include an action,
  camera image and optional sound. Alerts can also react directly to Home
  Assistant event or state entities. MQTT Discovery exposes the panel as a
  notify device with status and IP entities; see [notifications](notifications.md).
- A double tap on the clock shows the last HTTP/MQTT notification again (RAM only, no alerts, no sound or action).

### Updates and rollback

??? info "Device information and updates"

    **Device information and updates:** the editor shows logs, CPU and memory,
      restart reason, component versions and crash-dump controls. The panel checks
      this fork's GitHub releases for matching OTA images and offers a URL; users
      initiate installation separately. See [web editor](editor.md). When the URL is the release asset the check found,
      the panel compares the SHA-256 of the whole download with the digest GitHub reports and installs nothing on a
      mismatch (`sha256_verified` in `/api/ota/status`); a manually entered URL is not covered by that release-asset digest check.

??? info "Automatic updates (optional)"

    **Automatic updates (optional):** "Install updates automatically at night" (Settings > Firmware, off by default, with a warning when turned on). When nobody has used the panel or the editor for 10 minutes, between 03:00 and 04:00 local time (each panel at its own minute), with a good Wi-Fi signal and enough free memory, a panel whose bootloader supports rollback installs the newer release the daily check found, pre-releases included when the check includes them. It tries once a day. If the new firmware fails and the panel returns to the previous one, that version is skipped from then on and the panel shows a notice; the editor says what the last automatic update came to. Downloads are checked against GitHub's checksum, not a signature.

- **Version checks:** this fork's update checker compares complete version
  strings, so a newer pre-release or final release is eligible without the
  upload-time delay used by a plain version string. Pre-releases require
  **Include pre-releases**.

- **Chip-revision checks:** OTA uploads and downloads reject an image whose
  declared revision range excludes the device's ESP32-P4 revision before
  writing it. This does not make images for different display profiles interchangeable.
  Device settings show the actual chip revision and the firmware family
  (pre-v3/v1.x or v3.x), rather than a technical upper bound such as v1.99.

- **Rollback verified:** on a panel with the new bootloader a deliberately crashing image was installed over the air and the panel returned to the previous firmware by itself.

- **Bootloader shown in the editor:** Settings > Device says whether the panel's bootloader supports OTA rollback; its build date remains in the technical API data. An old one needs one USB flash (see development.md). Device memory labels distinguish internal RAM from external PSRAM.

??? info "OTA rollback protection"

    **OTA rollback protection:** a freshly installed image must run 60 s with the panel's own HTTP server answering
      and Wi-Fi (or the setup access point) up before it is marked valid; a reset earlier returns to the previous image
      (`ota_image_state` in `/api/version`). It needs a bootloader built with the option, so flash the bootloader over USB
      once per panel. NVS credentials survive `scripts/build.sh <variant> -p <port> flash`;
      layout/settings files survive only if LittleFS stays in place or is migrated
      as described above. A panel with an
      older bootloader installs new images by OTA exactly as before, without rollback protection.

### Security and reliability

- **Wallbox sessions:** cache timestamps are read before entering the critical section. This prevents a panic when an evcc session refresh competes for the system clock's mutex.

- **Home Assistant authentication:** a rejected token stops repeated logins
  that could otherwise ban the panel; the editor reports the problem.
  Optional admin authentication, request validation, a Content-Security-Policy
  and scoped credential handling harden the local HTTP editor and API. The
  first-run setup access point now uses a random WPA2 passphrase of two words
  and two digits (`maple-river-42`) shown on the panel. See [Security](SECURITY.md).

An unreadable admin credential closes the protected routes with 503 instead of disabling
authentication. Every fix is in [Security fixes](security-fixes.md); firmware stays unsigned and the
editor uses plain HTTP ([Security](SECURITY.md)).

??? info "Settings, credentials and transport safeguards"

    - Read/allocation errors preserve settings and layouts; secret generations commit before endpoint changes. After connection/credential changes, downgrades require re-entry ([storage](hardware/storage.md)).
    - Layout, language, style, text size and palette reload under the display lock, with retry after contention or a full queue. Settings, themes, translations and local graph histories use atomic file replacement; graph storage has a quota and orphan cleanup.
    - Automatic OTA requires a committed attempt record; storage errors retain blocked releases. Uploads have idle/total deadlines, and validation aborts when the flash gate is unavailable.
    - HA history and image-fetch tasks publish LVGL results under the display
      lock. Home Assistant credentials are scoped to its server; external image
      hosts do not receive the token. PNG input is bounds checked, and HTTPS OTA
      downloads are not redirected onto plain HTTP.
    - Full 32-character SSIDs and raw 64-digit Wi-Fi keys are supported; setup
      uses the word-based WPA2 passphrase shown on the panel.
    - ESP32-C6 recovery waits for active scan/status RPCs before stopping its
      transport. The C6 updater limits repeat attempts to avoid reboot loops.

- **Recovery:** a crash-loop guard starts with the built-in layout after
  repeated crashes, leaving the saved layout intact. P4 boards can store and
  download a panic dump when their partition table includes that slot. LittleFS keeps the two newest reports,
  each with download, details and confirmed erase. A yellow top-bar triangle remains until all reports are erased.
  Reports retain their ELF hash and available build/time metadata; PPA recovery records geometry and buffers,
  and UI stalls carry a readable cause. The original `paneljc` uses CPU rotation to bypass recurring PPA timeouts;
  long-run stability remains under observation. Other profiles retain their existing rotation paths.
  Wi-Fi recovery records survive resets; bounded ESP-Hosted send waits avoid an indefinite queue
  stall. See [coredumps](coredump.md) and [hardware](choose-display.md).

??? info "Network watchdog"

    **Network watchdog:** the panel restarts itself (which also resets the ESP32-C6) when a configured network stays dead: no Wi-Fi link for 10 minutes, or a link that looks up while the gateway, which answered pings before, stays silent for 10 minutes with no Home Assistant connection to show traffic flows. Never in the first 5 minutes after boot, not in setup AP mode, and at most three times in a row, so a router outage cannot cause a reboot loop. A third rule restarts the panel when its own web server, probed over loopback, stops answering for 5 minutes (also in setup AP mode). Restarts are recorded as "network watchdog" or "HTTP watchdog" in the restart history; state in `net_watchdog` of `/api/version`.

- **UI stall watchdog:** when the screen stops updating while the rest of the panel still works (a blocked UI task), the panel writes a core dump of all tasks and restarts after 90 seconds, and records it as "UI watchdog" in the restart history. At most three times in a row.

- **Restart history:** the panel keeps the last eight restarts with why each happened (settings restart, firmware update, factory reset, coprocessor update, panic, watchdog, power-on, brownout), how long the run before it lasted and its lowest free internal RAM. A breadcrumb in RTC memory survives a software reset, a panic and a watchdog reset (not a power cut) and is written to NVS once per boot. Shown under Settings > Device, as `GET /api/reboot-log` and as `last_reboot` in `/api/version`.

### Compatibility changes

- **ESP32-S3 support removed:** the `panels3` variant (Guition ESP32-S3-4848S040, 480 x 480) is gone, with its drivers, partition table, defaults and the editor's compact (480 px) layout paths. There is no hardware for it, and its screen cannot hold the newer widgets such as the weather forecast. v0.8.2 images of it stay in the upstream history.

- **Spanish and French removed:** the built-in languages are English and German (the editor and the device); the Spanish and French tables were outdated and unmaintained. A panel set to `es` or `fr` shows English until another language is chosen, or a translation file uploaded for that code is used. Custom translations for any language still work.

- **Vacuum rooms and Locate:** the Roborock tile's room-selection popup (cleaning single rooms through `roborock.get_maps`) and its Locate button are not part of the general Vacuum tile. The map from an optional image entity is shown inside the tile.

- **Quick Setup:** the first-run starter wizard of the editor is replaced by the add-widget dialog and the room and house page imports.

- **Removed-profile cleanup:** the editor no longer labels the retired S3 board; its camera help and device label now cover Tab5. The gallery lists sizes for supported P4 panels only. The camera-component vendor tool preserves both the JC OV02C10 and Tab5 SC202CS drivers during updates.

### Under the hood

Build, memory and development changes are preserved in [Release notes architecture](release-notes-architecture.md).
