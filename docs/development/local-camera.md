<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Local camera

User setup: [Cameras](../cameras.md). The Waveshare LCD-X implementation adapts the
feature set of [BETTA-HA-PANEL-10-Guiton-JC8012P4A1C at 7668ebe6](https://github.com/betamoojw/BETTA-HA-PANEL-10-Guiton-JC8012P4A1C/tree/7668ebe60ed736c838e0ea5b565a5e77df0b0b2f).
The reference uses OV02C10 and different ISP controls. Its camera task and board
initialization are not installed alongside the LCD-X capture owner.

## Feature mapping

| Reference feature | LCD-X implementation |
| --- | --- |
| Camera enable and JPEG snapshot | Explicit image opt-in, `/api/camera/snapshot` |
| MJPEG output | Separate opt-in, `/api/camera/stream`, maximum 2 fps |
| Local preview | Browser 30-second preview; native Camera tile and full-screen view |
| Native/half size | Existing OV5647 800 × 640 capture; optional 400 × 320 output |
| JPEG quality, horizontal/vertical flips | Quality 10–95; output and motion-grid mirroring |
| Brightness, contrast, saturation, hue, white balance | Software RGB565 processing; sensor AE stays unchanged |
| Sharpening, denoise, shadows/highlights | Bounded software filtering/tone adjustments |
| Threshold, area, duration, cooldown, startup delay | Pure host-tested motion policy |
| Four motion zones and lighting suppression | Mirrored 16 × 16 luma grid; zone union and per-zone diagnostics |
| Four HA/IP cameras | Saved source registry; HA JPEG proxy or explicit HTTP(S) JPEG URL |
| Settings persistence | Separate atomic LittleFS file; editor backup/restore integration |
| Display rotation adaptation | LCD-X output follows applied native pose; cached frames and zone overlays follow it |

Software filters are equivalents of the reference controls, not identical ISP
algorithms or calibrated optical results. The reference's 1920 × 1080 / 960 × 540
OV02C10 modes and 32 × 32 motion grid are not claimed for OV5647. Higher resolution
would require a new sensor/ISP mode and bandwidth measurements on LCD-X.

## Capture and memory

`drivers/ambient_camera.c` remains the only owner of V4L2 `/dev/video0`, CSI/ISP,
and the two RGB565 capture buffers. It retains the shared BSP I2C bus and existing
core assignment used by the LCD-X display/camera DMA pair. Light calculation reads
the unadjusted grid and sensor exposure/gain. Advanced motion runs independently
of image output; Display policy still enforces approach/night-wake permissions.

`camera/camera_output.c` asks this owner for pictures on demand. Demand expires on
the device four seconds after the last picture request. A snapshot waits at most
three seconds and accepts only a fresh cache entry from the current configuration
generation. Disabling output rejects new requests; encoding results are checked
again before publishing. No frame is written to flash.

RGB transformation and JPEG hardware encoding run outside the display lock. At
native size, temporary RGB and encoded buffers require about 2 MiB in addition to
the two existing capture buffers and cached JPEG. Allocations use PSRAM-capable
JPEG buffers; allocation/encoding failures return unavailable. One asynchronous
HTTP image worker keeps the main server responsive; simultaneous image clients
receive a busy response. Its stream ends on disconnect, opt-out or repeated failure.

Native views reuse the bounded camera worker/mailbox and decoder described in
[Camera streaming](camera-streaming.md). `betta-camera:local` or a saved source ID
selects the new snapshot fetcher. They do not pass through the HA-origin URL builder.
Workers use PSRAM stacks and never call LVGL. UI timers check the output permission
without waiting on storage, cancel stale work and clear old frames on opt-out or
configuration changes. Hidden views and full-screen ownership retain existing rules.

`camera_output_rotation()` reads the atomic applied display pose, so a pending
rotation held by a popup does not turn the camera early. LCD-X native zero
(user rotation 90) is the raw-camera reference. RGB565 correction uses the inverse
of the applied native angle: native 90 needs image 270, and native 270 needs image
90. This compensates the sensor's physical turn when the owner turns the panel to
keep the UI upright. Rendering combines downsampling,
sensor-axis mirrors, image controls and rotation in one pass without another frame
buffer. Quarter turns swap JPEG dimensions. Cache lookup and encoder completion
reject frames from a different pose. IP/HA source images retain their own orientation.

Motion remains in mirrored sensor coordinates; rotation alone cannot create motion
or move a saved detection area. The browser rotates rectangle overlays and inversely
maps pointer selections. Snapshot `X-Camera-Rotation` describes the returned picture;
status/config expose `rotation_degrees`. Preview aspect ratio follows the image,
and rotation changes do not extend the preview deadline.

## Configuration and API

`camera/camera_config.c` validates types, ranges, zones, IDs and URLs. The store
publishes a new generation only after `atomic_file_write()` succeeds. Startup
defaults do not create a file or overwrite a damaged file. Existing runtime
settings, layouts, themes, language, rotation and local backup files are untouched.

`/littlefs/cameras.json` contains local settings plus four source records. Passwords
are retained by stable ID when omitted; changing a credential's destination clears
it. The device file contains the configured secret, like other local credentials;
API reads and downloaded panel backups redact it. A backup restored to a new device
therefore needs its source passwords entered again.

| Route | Purpose |
| --- | --- |
| GET/PUT `/api/camera/config` | Settings, capability flags and defaults; PUT saves a partial local update or replaces the source list |
| GET/PUT `/api/cameras` | Alias for the same settings document |
| GET `/api/camera/status` or `/api/camera/motion` | Scalar status, test countdown and per-zone diagnostics |
| GET `/api/camera/snapshot?id=local` | Built-in JPEG; a saved ID selects that source |
| GET `/api/camera/stream` | Built-in MJPEG, requiring both output switches |
| POST `/api/camera/test` | Existing 30-second sensor test, no request body |

All routes use `http_guard_handle`; the guard's authentication and activity
accounting are unchanged. Status polling can therefore count as editor activity;
close the Cameras page before testing idle wake behaviour. Image responses are
`no-store`. Source HTTP requests reject redirects, cap JPEGs at 1 MiB, validate
complete JPEG markers and bound reads. HA credentials are used only for its
configured proxy; other URLs receive only their own optional Basic credentials.
Only panel10 enables built-in JPEG output. Other variants keep their existing
sensor behaviour and can use saved external sources.

## Checks

- Host tests: config validation/defaults/password handling, motion zones/debounce,
  image transforms, actual ambient driver, output gating/expiry, credential isolation,
  malformed JPEGs, stream cancellation and memory ownership.
- Web tests: real editing events to saved JSON, reload/reset, source tile binding,
  opt-in defaults, preview expiry and backup round trips.
- `tools/check_camera_editor.py`: real browser, synthetic local server, controls
  within their own groups at 1440 and 390 pixels; optional `--output` captures.
- LVGL `--regression controls`: native camera/full-screen lifecycle and clearing
  pictures when output is disabled, using synthetic frames.
- ESP-IDF 6.1 `panel10` build. None of these checks establishes physical capture
  quality, sustained frame rate, DMA coexistence or device stability with output on.

Before hardware acceptance, preserve settings/layout/backups, confirm source stamp
and valid OTA state, then exercise manual output opt-in, both sizes, stream stop,
image controls, each zone, IP/HA source authentication and source removal. Recheck
light/approach/night wake with output both off and on, and record internal heap,
camera task stack, watchdog and crash status. Preserve English, Classic Emoji and
the owner's current screen/touch orientation; confirm physical behaviour with the owner.

On 2026-10-28, the COM24 LCD-X unit passed four-angle JPEG decoding at both sizes
and three MJPEG frames per angle. The real browser followed all four output sizes
and stopped its preview after 30.33 seconds without extending the deadline.
The app-only build (`6298ed52-dirty`, image SHA256 prefix `f4e0f933`) ran from
`ota_1` with valid OTA state; its served editor bundle matched the build byte for
byte. Settings, layout, voice preferences and recorded local backups were preserved.
The only restart in this trial was the expected OTA restart, with no retained crash.
The output regression also checks source-pixel corners after sensor-pose correction:
identical frame dimensions cannot distinguish an upright image from an inverted one.
The owner confirmed upright, correctly proportioned pictures at all four display
angles after the correction, then restored the 90-degree setting. Its layout
projection matched the pre-update 90-degree layout exactly.

The same build passed the built-in snapshot/live tile and full-screen trial at
display setting 90. The owner confirmed pictures, orientation, touch and two
open/close cycles. Page hiding stopped the image worker and capture demand;
returning restarted them. The temporary page was removed and the six original
pages matched the backup. The 485-second trial had no reboot or retained crash;
resource readings and scope are in [Camera streaming](camera-streaming.md).
