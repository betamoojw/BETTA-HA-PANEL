<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Components and patches

## Module ownership

[Development](../development.md#architecture) maps boot, configuration, networking, API, display and editor.
Home Assistant/MQTT/Wi-Fi feed the model/event queue and `ui_runtime.c`; API layout saves rebuild the UI.

| Responsibility | Source under main/ |
| --- | --- |
| HA transport/task, layout/settings loading and overflow logs | `ha/ha_client.c` |
| Domain attribute compaction and forecast extraction | `ha/ha_attr_serialize.c` (host-tested) |
| Layout-driven subscriptions | `ha/ha_layout_entities.c` |
| Area/floor registry and display precision | `ha/ha_room_registry.c`, `ha/ha_display_precision.c` |
| Editor discovery domains (one table entry per new domain) | `ha/ha_discovery_domains.c` |
| Fallback names and parent-device areas | `ha/ha_registry_discovery.c` (host-tested) |
| Bounded effect/fan options and escaped service values | `ha/ha_control_options.c` (host-tested) |
| Seven-day history and Energy Gas/Water/Solar statistics | `ha/ha_history_stats.c` (pure, host-tested), `ha/ha_history_week.c` (serialized WebSocket worker) |
| Daily bars in popups and Energy details | `ui/ui_days_view.c` |
| Weather parsing | `ui/widgets/weather_value_parse.c` |
| Weather rendering entry points and private modules | `ui/widgets/w_weather_tile.c`, `weather_tile_{lottie,text,icon,values,forecast,layout,popup,render}.c`, `weather_tile_internal.h` |
| Shared firmware/simulator House and Energy configuration | `ui/ui_house_page.c`, `ui_energy_page_config_from_json()` |
| Brightness/wake/inactivity, pure policy | `drivers/display_brightness.c`, `drivers/display_policy.c` |

Statistics timestamps are UTC; day boundaries follow the panel's local zone. Measured reply shapes are in
[Recorder statistics](learnings.md#recorder-statistics). Rendering refactors need before/after SDL hashes
(`SDL_VIDEODRIVER=dummy`, simulator `--screenshot`), including the weather modules.
The WebUI build bundles/fingerprints ordered JS and CSS, gzips and embeds three assets; see
[Web editor source](../editor-source.md). Build-only bundles are not maintained source modules.

## Vendored camera component and the P4 silicon cap

`components/esp_cam_sensor` is Espressif's core plus OV02C10 (JC8012P4A1), OV5647 (LCD-X, with read-only AEC/AGC statistics) and SC202CS/SC2356 drivers
(Tab5), cut down to those sensors and the DW9714 motor. `tools/vendor_cam_sensor.py <version>` stages a new registry core
and keeps all three sensor directories and `project_include.cmake`; `esp_video` pins the matching line (2.3.x wants 2.3.*).
After a version change update the `esp_cam_sensor`, `esp_video`, `esp_ipa` and `esp_h264` entries in the variant lock
files, rebuild the affected camera variants and check each camera on its panel (`POST /api/camera/test`, the
`ambient_cam: luma ...` log line). Tab5 awaits its first device test.

Panel10 also pins `esp_sccb_intf` to 0.0.9 so resolving the vendored camera's
open dependency range does not silently upgrade its SCCB interface driver.

Versions stop at `esp_video` 2.3.0, `esp_ipa` 2.2.0~1 and `esp_cam_sensor` 2.3.0 on purpose: the prebuilt `libesp_ipa.a`
of esp_ipa 2.3.0+ contains RISC-V Zba/Zbb instructions (`sh2add`, ...; `objdump` counts 0 up to 2.2.0, 163 in 2.3.0, 193
in 2.4.0) that P4 rev v1.x silicon does not execute. A trial with esp_video 2.5.0 crashed the camera task with an illegal
instruction 20 s after boot (`create_ipa_array`; the rollback bootloader restored the previous image). ESP-IDF treats P4
rev < 3.0 and ≥ 3.0 as mutually exclusive builds (`ESP32P4_SELECTS_REV_LESS_V3`), so the newer line needs a separate build
for v3.x boards. `MAX_LINE` in `tools/check_component_updates.py` holds the caps; a prebuilt library cannot be told to
avoid the instructions.

## Shared LVGL adapter

Every panel registers display and touch with `esp_lvgl_adapter` 0.7.5 through
`main/drivers/display_port.c`. Board files initialize hardware only. The shared
adapter task uses priority 20, core 1 and the existing internal-RAM stack; LVGL
allocations, two partial draw buffers and rotation scratch use PSRAM first.

The adapter's registration rotation stays zero. Its custom bitmap callback uses
the current `lv_display_get_rotation()` for both boot and runtime orientation,
keeping Tab5 automatic rotation and LVGL's native touch mapping together. Native
DPI framebuffer counts remain unchanged; no extra full-screen buffer is required.
`APP_DISPLAY_PPA` selects bounded PPA rotation or LVGL CPU rotation; P4 profiles use the PPA except LCD-X Panel10, which retains CPU rotation and synchronized frame presentation. S3 uses CPU rotation.
A PPA completion has a one-second deadline and retains crash evidence; uncertain
output is never submitted or reused after a timeout. The adapter's sleep handling
is disabled because BETTA owns brightness, wake and inactivity policy.

Some BSPs still depend on `esp_lvgl_port` for their own convenience functions.
It may appear in the resolved component list, but BETTA does not initialize its
task or register displays/touch through it. Panel10 and JC V2 share the new source
path but have no hardware qualification in this fork.

Device acceptance on 2026-10-13, source `650fd258`: panel4, original paneljc and
one ST7121/v1.3 Tab5 report adapter 0.7.3, valid OTA, live Home Assistant connection
and unchanged layouts/settings. The owner confirmed physical image orientation
and page navigation by touch on all three. Tab5 was mounted at 270 degrees;
this check does not qualify physical touch mapping in every automatic orientation.

## Chip temperature diagnostics

All P4 variants expose `chip_temperature` through `GET /api/version`: `available`,
`celsius`, `sample_age_ms` and `error`. This is internal chip temperature, not room
or case temperature. The IDF sensor uses its 20–100 °C range (nominal error up to 2 °C).
`util/chip_temperature.c` samples once at startup and every ten seconds on the existing
ESP timer task. HTTP reads only the cached snapshot. Read failures set `available=false`
and `celsius=null`; age refers to the last successful sample, or -1 if none exists.
Initialization failure logs once and never prevents boot. No UI field, HA entity,
new task, temperature alarm or persistent history is added.

## Original JC display timing

The original `paneljc` build includes `display_timing` in `/api/version`. It measures BETTA's lock
waits and outermost recursive holds, LVGL refresh cycles, rotation, the bitmap callback,
transfer submission and actual DMA completion separately. Each metric reports count,
total/max milliseconds, slow count (100 ms or longer), errors and the maximum's caller PC.
The last 16 slow operations or lock failures retain uptime, duration, caller/owner PCs,
known application-owner task and whether a refresh was active. Decode PCs against that
running image's archived ELF. Aggregates overlap and must not be added as CPU time.

Application-owner tracking covers `display_lock()` calls, not the adapter's private mutex
operations; `untracked` is not proof that the mutex was free. Refresh duration includes
waiting for partial transfers, not just CPU drawing. The normal lock deadlines and
rendering remain unchanged. Minute warnings summarize new timeouts/slow records;
there is no per-frame log, WebUI row or persistent history. ISR completion accounting uses
fixed internal RAM and IRAM code; JSON snapshots use a short copy and PSRAM allocation.
`display_timing_dpi.c` wraps only the DMA-completion callback, preserving all other callbacks,
user context and callback return values. No frame sampler or DSI register overrides are installed.

## Incremental model recovery

HA states carry a cache revision in a separate PSRAM table. `ha_model_changes()` returns
small ordered batches of the latest changed entities. Normal entity events still apply
immediately; the UI remembers applied slot revisions and skips them during recovery.
It processes at most one unapplied entity per UI turn, releasing the display lock between
entries. A failed lock retains both the entry and its cursor. Updates arriving during a
batch remain eligible for the next batch. Rename revisions and cache reset are covered.
The cursor uses serial uint32 ordering (consumer lag below 2^31 revisions).

This replaces the periodic full-layout replay, which held the JC's display lock for about
two seconds. Initial layout priming, HA reconnect and the exceptional failed-event fallback
keep their existing complete refresh paths. Single entity/widget application can still be
slow; timing diagnostics distinguish that from the removed periodic sweep.

## Vendored PPA driver (temporary)

`components/esp_driver_ppa` is ESP-IDF 6.1's driver with upstream commit `469aa16` (esp-idf#19096, #19023): the
P4 DIG-734 workaround now uses the rotated block size, so 90/270 degree RGB565 rotations no longer fail to complete.
It overrides the IDF component for all variants. Remove it, with its `LICENSE` entry, once the ESP-IDF in use
contains the change: `grep -n "in_block_w" $IDF_PATH/components/esp_driver_ppa/src/ppa_srm.c` finds the fixed code.
After removing it, rebuild every variant, run a rotation soak on paneljc and tab5 and check that `GET /api/version`
reports no PPA coredump.

## Waveshare LCD-4B BSP override

Panel4 uses BSP 3.0.1 from `third_party/waveshare__esp32_p4_wifi6_touch_lcd_4b`.
The official source and Apache-2.0 license are preserved; `BETTA.md` records the
registry archive hash and repository commit. Only the adapter manifest constraint
changes from `~0.6` to `0.7.5`, matching the shared runtime and LVGL 9.6.
Dependency solving runs before the configure patch hook, so this override belongs
in the manifest rather than a manual edit of `managed_components/`.

The profile explicitly selects 720 × 720 and two DPI framebuffers. The ST7703
constructor already enables IDF 6's DMA2D copy hook; do not enable it again in
the board initialization. BETTA retains GPIO23 touch reset with the 0x5D/0x14 probe;
the new BSP declares the reset pin NC. Display timing and the separate ES8311
chime path remain board-specific.

## Waveshare LCD-X BSP override

Panel10 targets the ready-made ESP32-P4-WIFI6-Touch-LCD-10.1 through Waveshare
BSP 2.0.3 in `third_party/waveshare__esp32_p4_wifi6_touch_lcd_x`. Its `BETTA.md`
records the official archive hash and local adapter/DSI-clock changes. The preset
selects P4 Rev3.0+, JD9365, RGB565, two DPI framebuffers and fixed 270-degree
landscape. GT9271 uses a non-null BSP configuration and the GT911-family driver;
LVGL applies rotation. PWM brightness uses GPIO26; touch reset/interrupt stay NC.
The shared adapter/PPA runtime is retained. Optional OV5647 sensing shares the
BSP I2C bus and uses Espressif's original sensor modes/ISP profile with a small
read-only metering extension. ES8311 chimes use the existing transient output
path and BSP pins. Microphone capture is not initialized. Both runtime features
start off; manufacturer examples and source hashes are recorded in the vendor notes.
This replaces the inherited Nano BSP and its compatibility patches; the Nano
plus separate DSI kit is no longer the panel10 target. Hardware validation is pending.

## Patches to fetched components

`managed_components/` is git-ignored and overwritten whenever the component manager refetches (manifest change, or a
build at a commit whose manifest differs from the lock). The small edits needed there live in
`tools/apply_component_patches.py`, never by hand:

- two esp_hosted SDIO fixes (byte-mode TX workaround, bounded `esp_hosted_tx()` queue wait instead of an infinite one),
- compatibility fixes for the older fetched Waveshare BSPs, including panel10,
- serialized WebSocket polling/close, post-lock connection checks and stalled-frame
  handling in `tools/patch_websocket_transport.py`,
- a bounded wait for the PPA screen rotation in `esp_lvgl_port` (see
  [UI stall watchdog](learnings.md#watchdogs-and-crash-evidence)).

The root `CMakeLists.txt` runs the script on every configure, right after `project()`. Check or apply outside a build:

```bash
tools/apply_component_patches.py --check   # what is applied, what is missing (touches nothing)
tools/apply_component_patches.py           # apply without a build
```

- A build fails with `FATAL_ERROR` naming the script when a patch's place is not found: a component version moved it;
  adjust the `PATCHES` list.
- **Do not build an old commit in this checkout.** `PATCHES` matches the current component layout; an old
  `dependencies.lock` can pull older versions and the hook then reports a false `MISSING`. Use a clone or worktree.
- **Cover requests** keep a separate cancellation ticket until their final LVGL callback completes
  (`ha/cover_request_lifetime.c`); destroying a widget cancels all its tickets and a new widget at the same heap address
  gets fresh ones. This fixed the Tab5 rotation trial of 2026-10-05: rebuilt media widgets hit a late callback into a
  deleted context (`tlsf_free` assertion in `mp_release_cover`, OTA rollback after 20 s; the panic PC read "Illegal
  instruction", the decoded core dump showed the double free).

### WebSocket TLS ownership

Keep `CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK` disabled: TLS receive-buffer replacement,
send, polling and close access the same TLS context. On COM24, `de6dc171` crashed
on 2026-11-06 at `ssl_check_ctr_renegotiate()` reading a null receive counter during
audio upload (matching ELF `bd29dfb0f`, dump `9d73336a83529206...`). This is separate
from the owner's successful acoustic test. The exact receive-side interleaving was
not captured; the unprotected shared context and null read were confirmed.

The component patch uses the shared lock for a 20 ms poll and yields after each
receive pass. Sends recheck the connection after acquiring the lock. A zero-progress
nonempty frame fails, and successive reads share the configured receive-time budget;
individual transport calls can still consume their own timeout. AI Assistant uses
20 ms lock attempts, a 500 ms transport-write budget and a 5 s network timeout.
The fork send entry point distinguishes lock contention before any bytes are written
from a failed or partial write. A bounded PSRAM outbox retries only that pre-write
case, with a fixed 5 s deadline from enqueue; partial writes are never replayed.
Any short transport payload aborts the incomplete frame instead of starting a
continuation frame. Transport failures cannot return the pre-write busy sentinel.
The upstream public send API retains its original timeout and return contract.

The worker frees an incoming queue slot and attempts queued sends before waiting
for playback space. This lets the receive callback release TLS before the next
audio frame blocks the worker. Receive waits check cancellation every 50 ms and
fail after five seconds without space; playback waits up to 150 ms. The 250 ms
receive deadline failed a real long-reply trial. Voice session/capture/playback
tasks use core 0 to avoid sharing core 1 with priority-20 LVGL. Sustained stalls
still fail. TLS serialization, queue limits,
audio task ownership and normal-close error precedence remain intact. See the
[COM24 interruption investigation](assistant-transport-validation.md).
Teardown marks the connection stopping before joining audio tasks, so a full
receive queue cancels promptly without a false stall error during a local stop.

Regressions compile the fetched component's actual send/receive/poll functions with
injected scheduling, partial-frame and timeout faults, including negative controls
against the original implementation:
`python -m unittest discover -s tests/tools -p test_websocket_transport.py`
(host C compiler via `CC`). Regenerate an existing
saved sdkconfig from project defaults; panel10 rejects the unsafe old lock setting.

### AI Assistant connection endings {#xiaozhi-connection-endings}

The application reads the raw CLOSE payload in the DATA callback. In WebSocket
client 1.8.0 this callback runs before `close_status_code` is updated; that field
is usable in the later CLOSED event. Codes 1000/1001 and a valid empty CLOSE
return to standby. Other codes, malformed frames, transport failures, unexpected
disconnects and send/handshake timeouts retain the error path. An observed normal
CLOSE suppresses only the send rejection caused by closing while upload waits on
the shared lock. Earlier errors and errors delivered while stopping remain errors;
even an intentional user/idle stop retains a concurrent fault's diagnostics.

`GET /api/xiaozhi` retains `connection_end`,
`connection_close_code` (0 for no code), transport error type, TLS/socket errors
and HTTP handshake status across the next wake. Before any fault these describe
normal peer closes; once a fault occurs, normal closes preserve its details and
only a later fault replaces them. `normal_closes` and
`connection_errors` count peer endings/failures since boot; local user/idle stops
are excluded. Logs record these numeric diagnostics without server reason text
or credentials. A new wake clears active transport state and opens a new connection.
The [session policy](assistant-session.md) separately reports local farewell,
silence and manual endings; those reasons do not replace retained fault details.

`python -m unittest discover -s tests/tools -p test_xiaozhi_connection.py`
compiles the actual callback, send, teardown and worker-ending functions. It covers normal
and abnormal closes, malformed payloads, queued sends, late errors, timeouts and
reconnection, with negative controls for ignored CLOSE, stale codes and early
diagnostic snapshots. These injected host tests do not measure Wi-Fi reliability.

### Variant lock files

Each dependency set has a committed lock file, selected by the root `CMakeLists.txt` and used for the component
versions reported by `/api/version`:

| Variant | Lock file |
| --- | --- |
| Panel4 | `dependencies.lock` |
| Panel10 | `dependencies.lock.panel10` |
| JC V1 and V2 | `dependencies.lock.paneljc` |
| Tab5 | `dependencies.lock.tab5` |

The manifests use different BSPs and can resolve different transitive versions. Building one variant must not
replace another variant's lock. Builds still share `managed_components/`, so build variants serially. Review and
commit the matching lock when changing a manifest; use a disposable checkout for dependency experiments.

## Keeping components current

`python3 tools/check_component_updates.py` lists each managed component with locked version, constraint and newest
registry version (see `tools/README.md`). Run it before touching `dependencies.lock*` or the manifests.

## Dependency pins

Variant manifests and lock files are authoritative. JC/Tab5 select `esp_sccb_intf` 1.0.0 and
`libpng` 1.6.59; Panel10 has its own lock at 1.6.59, while Panel4 retains 1.6.58~2.
The Waveshare BSP's split ESP-IDF driver dependencies are supplied during the first configure;
a second configure is not required to discover missing driver components. Shared build/CI scripts
may still reconfigure deliberately for their other responsibilities.
