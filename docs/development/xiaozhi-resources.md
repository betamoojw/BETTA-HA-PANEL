<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi resources

## HTTP connections

The editor previously allowed 12 HTTP sessions against 16 global lwIP sockets.
HTTPD itself needs three more descriptors. HA, voice, MQTT and health probes
could exhaust the global pool before HTTPD reached its own LRU threshold:
`httpd_accept_conn: error in accept (23)` was observed on COM24.

Default profiles now allow 32 sockets. `http_socket_budget.h` caps HTTP clients
at 12 and reserves at least 12 descriptors, including HTTPD's three internal
sockets and nine for outbound services. An existing 16-socket sdkconfig uses
four HTTP clients until reconfigured. The startup log reports both limits.
The Xiaozhi status poll allows one request at a time, aborts after eight seconds
and rejects a response that predates an edit. Command ordering is unchanged.

## Local transcript font

The compact embedded CJK subset remains the UI fallback. Free-form recognized
speech and AI replies can instead use a pinned Noto Sans SC 2.004 resource with
29,417 codepoints. It includes 20,976 unified ideographs through U+9FEF and
6,582 Extension A characters through U+4DB5, plus punctuation and additional
source glyphs. This is not all Unicode or regional Traditional Chinese shaping.

The maintained `main/ui/fonts/noto_cjk_reply.ttf.gz` is OFL-1.1, covered by
`NOTO_CJK-OFL.txt`. Its manifest identifies the source, weight, size and SHA256.
Regeneration uses the same pinned source as the compact font:

```bash
python tools/prepare_reply_font.py --source NotoSansSC-VF.ttf --check
```

The expanded file is 10,178,080 bytes. It is deliberately outside the application
image: the verified image preceding this change had only 180,320 bytes of OTA
headroom. No partition migration is needed. The panel reads outlines from
LittleFS through a private read-only LVGL drive with a 4 KiB file cache and
64 cached glyphs per size, rather than allocating the whole font in PSRAM.
Only the two transcript labels receive the extended fallback; existing widget
fonts and font chains are not mutated. New instances share the 96-font budget.

After an authorized firmware update, install the resource once from this checkout:

```bash
python tools/install_reply_font.py --url http://PANEL_IP --install
```

Omitting `--install` only checks status. For an editor protected by Basic auth,
pass `--username` and set `BETTA_EDITOR_PASSWORD` in the environment. The tool
never prints credentials, changes settings, deletes reports or restarts a panel.
The repository resource is verified before upload; no transcript leaves the
device for font lookup. After installation it also works without Internet access.

GET `/api/xiaozhi/font` reports readiness, expected identity and upload progress.
Guarded PUT accepts at most 32 KiB at the exact `X-Font-Offset`. The only target
is a versioned resource path; requests cannot choose arbitrary file names.
Uploads use the flash gate and retain at least 1 MiB free space. Partial writes
stay under a private `.part` name; rerunning the installer restarts that file.
The final hash must match the compiled identity before an atomic rename exposes
the resource. Existing final files are never overwritten. If a final resource
is corrupt, retain it for diagnosis before an explicit recovery operation.

Boot verifies the saved resource before UI/HTTP startup. A new upload becomes
available on the next dialogue refresh, without a reboot. Missing or invalid
resources retain the compact fallback; flashing the app alone does not install
the expanded font. Package the compressed resource, manifest, license and the
two Python tools alongside firmware releases that use this feature.

## Local checks

On 2026-10-25, 326 browser tests and web-source checks passed. New tests cover
single-flight polling, timeout recovery and stale replies after edits. Host
checks cover legacy/current socket budgets and resource bounds, interrupted
uploads, invalid hashes, immutable files and restart verification.
Real LVGL checked all 20,976 unified ideographs, Latin/emoji fallbacks, cache
reuse and a rendered simplified/traditional transcript sample. Native settings
and Xiaozhi overlay lifecycle regressions also passed.

## Waveshare P4 validation

On 2026-10-25, the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 on COM24 ran
the patched `0d61e0d6-dirty` image (ELF SHA256 prefix `314c4245`). The supported
32 MiB stub verified the application and unchanged metadata, coredump and
LittleFS checksums before the first boot. OTA validation succeeded. Startup
reported 12 HTTP client sessions and 32 global sockets.

The separate font installation completed with the expected SHA256. Installing
this resource intentionally adds a file to LittleFS; the unchanged-filesystem
claim applies only to the application flash before boot, not font installation.
Two bounded HTTP trials each held eight connections and issued 120 requests
from four concurrent workers; all 240 requests succeeded. The second trial
included a real voice session. Over 713 seconds, 318 status samples recorded
no polling failures or new crash archives. Restoring English through the
settings API caused one documented settings restart, not a crash. The resource
remained verified after that boot. Settings, layout and themes matched the
pre-flash baseline, including Classic Emoji, English and 180-degree rotation.
The owner confirmed clear simplified/traditional Chinese replies, normal voice
responses and automatic dialogue dismissal.

## Unresolved white flashes

The owner still observed occasional full-screen white flashes, including while
idle with Live Display closed. Screen stability has **not** passed; the HTTP
and font results above do not establish a display fix.

A subsequent 100-second passive COM24 UART capture contained no display
underrun, watchdog or crash errors, and the owner saw no flash during that
interval. A separate bounded HTTP comparison made 90 read-only settings,
version, Xiaozhi and layout requests without failures or owner-observed flashes.
Neither short negative trial rules out the reported intermittent fault.
Investigation stopped at the owner's request before the voice-specific UART
comparison completed. No display-driver change, new flash or settings change
was made during that follow-up. The root cause remains unconfirmed; keep this
limitation separate from the verified connection and Chinese-font changes.

On 2026-10-26 the owner again reported the same white flashes during the MCP
device-tool trial; the controls otherwise worked. Later bounded dialogue and
header-icon checks passed, but do not establish a fix. The issue remains open
for separate display investigation; these changes do not prove all-day stability.

## Custom wake recognition

The worker owns both WakeNet presets and the optional ESP-SR 2.4.7 `mn7_cn`
engine. `POST /api/xiaozhi` accepts `{"action":"wake_config","value":{"word":3,
"phrase":"ni hao guan jia"}}`; IDs 0–2 select the bundled presets, 3 selects
custom Chinese pinyin. `GET` returns `wake_word`, `wake_phrase` and
`wake_model_ready`. HTTP validates bounds/syntax, then returns 202 for the queued
request; read back the configuration to confirm application. The worker validates
with MultiNet before committing one `wake_config` NVS blob. Invalid saved blobs
fall back to the default. Invalid engine input reports `wake_config_error`.

The packed resource includes both `mn7_cn` and `fst/commands_cn.txt`. Its
minimal seed row uses the pinned component's comma format (`1,ni hao xiao zhi`).
ESP-SR 2.4.7 initialization requires that FST even though the worker immediately
replaces its commands; omitting it makes the vendor initializer dereference a
failed model creation. The loader checks for the FST before calling that code.

`xiaozhi_custom_wake.c` loads the verified packed model into PSRAM. One model
catalog exists at a time; engine destruction precedes catalog/buffer release.
A 0.20 threshold (the upstream custom-wake default) and command ID 1 gate
custom detections. Detection and timeout
clear MultiNet state; the known unsafe WakeNet clean hook remains unused.
The existing mono 16 kHz microphone path feeds detection; no idle audio is sent
upstream. This path does not add acoustic echo cancellation or noise suppression.

`prepare_custom_wake_model.py` verifies the pinned resource identity and builds
`build/panel10/xiaozhi-custom-wake.bin`. Packaging copies this build output as a
separate `.wake-model.bin`, without regenerating it from later dependency state.
GET/PUT `/api/xiaozhi/wake-model` uses authenticated, bounded
32 KiB chunks with `X-Model-Offset`, a temporary file, final SHA256 verification
and immutable rename. A 1 MiB storage reserve is required. The model is verified
again at boot before inference; no partition changes are needed.

For a local build, install with the editor file picker or:

```sh
python tools/install_custom_wake_model.py --url http://<panel-ip> --install
```

This installer does not enable the microphone or change preferences. Custom words
need a physical speech trial; host tests and a ready engine are not acoustic proof.

On 2026-10-31, the Waveshare 10.1-inch panel on COM24 ran ELF `db2854d06`
with resource SHA256 prefix `2edad3a5633b`. Serial readback matched the app image;
OTA state was valid and the custom phrase `ni hao xiao bei` survived restart.
Browser save/reload, wake disable/re-enable, all three presets, invalid syntax
and MultiNet rejection retained the expected settings. Layout and unrelated
settings were preserved. The device recorded a custom wake hit, recognized a
time query, received its answer and returned to local detection after inactivity.
During that dialogue the Xiaozhi task stack low-water was 6,912 bytes, internal
heap minimum 138,948 bytes and free PSRAM 3,731,896 bytes. These are bounded
panel10 measurements, not other-board or long-duration qualification.

The first trial exposed the missing seed FST described above. Its crash report
`679ce109` (ELF `997410f33`) was archived and retained on the device. The corrected
resource passed initialization and the subsequent checks without a new report;
the editor's retained-crash banner can therefore still refer to that earlier build.

The owner reported several seconds of wake latency with the initial 0.90
threshold. That build's hit counters established detections, not satisfactory
response time. The follow-up uses the upstream custom-wake threshold 0.20 and
logs detection, UI display and listening milestones. `loud_age_ms` measures time
since a microphone peak of at least 256; it is only an energy marker, not a
speech endpoint or an end-to-end acoustic latency measurement.

Follow-up ELF `2b71290cc` (source stamp `2026-10-31 12:29:30`) reached valid OTA
state on the same panel. Four trials recorded 81–205 ms from local detection to
the UI show call and 815–916 ms to listening. The first accepted score was 0.319;
a host regression now ensures that this result is accepted while low scores,
wrong command IDs and missing results are rejected. The UI lifecycle regression
passed. These software timings do not measure physical display onset. On
2026-10-31 the owner confirmed that, after finishing "ni hao xiao bei", the
dialogue appeared within approximately one second and was noticeably faster.
This completes the bounded custom-wake trial on this panel.

Editor saves wait for the worker's result revision, including when the saved
values already match. Commands expire after 10 seconds; model requests (including
response bodies) expire after 20 seconds and release the controls for retry.
An invalid custom draft remains editable when switching presets; it cannot block
saving a preset, which retains the last valid saved phrase instead.
