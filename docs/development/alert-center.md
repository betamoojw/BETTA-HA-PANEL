<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Alert center

`ui_alert_center` owns nine bounded, PSRAM-first slots: two retained crashes,
three connection failures and four editor-configured entity alerts. Only the
UI task or display-lock holder mutates the list or opens its LVGL overlay.
No worker calls LVGL. The overlay uses the shared popup close button and
scrolls without shrinking text. Rebuilding pages closes it; deleting the last
alert hides the header action and shows an empty state if the overlay is open.

`alert_policy` contains the pure connection and entity-edge rules. Connection
alerts wait 30 seconds after boot and ignore unconfigured services. Wi-Fi
failure suppresses duplicate HA/MQTT warnings. Usable entity states prime the
edge detector without replaying events; unknown states retain the last usable
sample. Sensor entries follow active state. Event entries expire by monotonic
uptime; configured timeouts are capped at one hour for the header list.

`ui_runtime_refresh_topbar` reconciles the list once per second, including
missed entity-change events. Crash metadata comes from
`coredump_archive_snapshot`: a non-blocking copy of a mutex-protected memory
cache refreshed after import/deletion. The UI never reads LittleFS or raw
flash while holding the display lock. Existing report deletion remains in the
web editor/API; opening or closing this screen does not erase anything.

## Checks

Run the simulator with `--regression alerts` and an empty layout fixture.
It checks connection grace/disabled/recovery, sensor startup and state edges,
event priming/reconnection, all seven languages, hidden/present icon, click
opening, duplicate-open prevention, expiry, live empty state and close/rebuild.
`screen-settings` additionally checks the alert is left of the AI action
across AP/MQTT/warning header combinations. `alert_demo` fixture data is synthetic.

Local checks on 2026-10-24 passed the alert and header regressions on 480 x 480,
720 x 720, 1280 x 800, 800 x 1280 and 720 x 1280, including larger text and all
seven languages. Close/scroll bounds, expiry, recovery and page rebuilds passed.
Static WebUI, generated catalogs, font subsets and strict documentation passed.
The Panel10 ESP-IDF 6.1 image built successfully; 17% app-partition space remains.

On 2026-10-24, the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 on COM24 ran
the `a4eeaa88-dirty` Alert image (application SHA256
`c1b208dad5c8b33ebfd8fca09534eec9b937f95c614bd3257f90d357d0572fff`).
The supported flasher stub wrote only the application; application readback
matched and protected storage checksums were unchanged before boot. Running
source/build identity matched and OTA state was valid.

The owner confirmed readable portrait alert details, working clicks and a
stable physical display. Captures showed both historical crash records with
wrapped details. Later readback contained no retained reports and the landscape
header hid the alert action. The owner confirmed deleting both reports in the
web editor, establishing automatic hiding after removal of the last alert on
this device; both original reports remain backed up locally. The alert-before-AI
order is covered by the local header regression, not a separate device capture.
Final settings retained English and 180 degrees; layout and themes matched the
latest snapshot. Boot sequence remained 99 through uptime 466 seconds, with no
new crash reports. The first monitor stopped on a decreasing report count;
its generic assertion message did not indicate a new crash. Follow-up sampling
confirmed increasing uptime and an unchanged boot sequence. Connection-failure
and entity-alert transitions remain simulator-tested rather than injected on
this configured device.

The same image includes the AI Agent robot header action. Its earlier COM24
click, portrait and physical-output checks are recorded in
[Xiaozhi AI](../xiaozhi-ai.md#validation-limits). These measurements describe
the pre-commit images identified above; committing the sources does not change
the build identity of the firmware already running on the panel.
