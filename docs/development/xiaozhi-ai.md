<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi AI

Protocol reference: [kruzio1985's Guition port](https://github.com/kruzio1985/BETTA-HA-PANEL-10-Guiton-JC8012P4A1C/tree/7668ebe60ed736c838e0ea5b565a5e77df0b0b2f/main/xiaozhi),
commit `7668ebe60ed736c838e0ea5b565a5e77df0b0b2f`. Its Cpt_Kirk/ForgeUI-derived
implementation uses activation POSTs, a WebSocket v3 hello and 60 ms mono Opus
frames. The local integration reimplements those boundaries rather than using
the Guition audio pins or its direct network-task LVGL calls.

## Runtime

- `main/xiaozhi/xiaozhi.c`: one worker owns pairing, connection lifetime, Opus
  and audio I/O. HTTP and touch callbacks enqueue commands. The WebSocket event
  task only assembles bounded frames into a finite queue; overflow ends the
  session. The shared MCP dispatcher advertises device tools and the configured
  [KNX objects](knx.md).
  Starting microphone capture discards stale speech frames but services queued
  MCP requests, preserving tool discovery even when initialize/tools/list arrive
  beside hello or TTS/stop. Disconnect still discards the whole old session.
- `main/knx/knx_mcp.c`: pure JSON-RPC dispatch, schema validation, bounded
  `tools/list` pagination and the ordered Common, KNX and User Only inventory.
  A `withUserTools: true` list request exposes annotated user tools and enables
  their calls for that initialized connection. See
  [Xiaozhi MCP tools](xiaozhi-mcp-tools.md).
- `main/xiaozhi/xiaozhi_mcp_tools.c`: `panel10` capability adapter for saved
  speaker volume, brightness and theme, device/system status, display geometry,
  reboot and the existing HTTPS OTA installer. Other variants return no device
  capabilities, so no voice tool is advertised on hardware without supported
  microphone and speaker drivers.
- `xiaozhi_audio.c`: LCD-X BSP pin definitions, ES8311 output and ES7210 input;
  MIC1 is selected at 16 kHz, excluding the playback-reference slot. Opus decodes server audio directly
  to 16 kHz. Audio opens lazily on conversation start or enabled local detection.
- `xiaozhi_ui.c`: a UI timer reads a locked status snapshot. A child root owns
  the controls and timer; cleaning or deleting the dashboard destroys both and
  requests disconnect. The persistent screen must not own that cleanup callback:
  layout reloads clean its children without deleting the screen. Network tasks
  never call LVGL. The view tracks whether it opened from a wake event or the
  header: detector readiness alone must not hide manual Talk. Header clicks do
  not send the automatic-listening `open` command; Talk press/release own manual
  capture. A later wake event switches the visible view to hands-free mode.
- `xiaozhi_api.c`: guarded GET/POST `/api/xiaozhi`, with bounded JSON commands.
  The API returns status, pairing code and latest transcript, never credentials.
- `components/webui/www/js/xiaozhi.js`: immediate-save controls, scoped reset,
  pointer/keyboard push-to-talk and cancellation on page exit.

Only `panel10` links `78/esp-opus` 1.0.5, `espressif/esp-sr` 2.4.7 and the audio implementation. Other
variants compile an unsupported implementation. The SDL path does not call
the hardware assistant.

Pairing uses HTTPS and the certificate bundle; WebSocket endpoints must use
WSS. A URL/token pair is committed in one NVS blob under `xiaozhi`; flash writes
use the shared OTA gate. The persisted UUID matches both activation and WS
headers. Pairing lasts at most 180 seconds and capture at most 60 seconds.
Pairing begins only after an explicit command. Enabling alone does not contact
the cloud. When independently enabled and paired, local wake detection runs
while idle; a detected wake starts a cloud conversation. A header click only
opens manual controls; capture starts on Talk press and ends on release or
press loss. Disabling can wait for the current bounded network call.

Codec storage, PCM, receive frames and queued payloads use PSRAM. The worker
stack is 32 KiB internal RAM because it performs NVS writes and Opus encoding;
it is allocated only when the feature is first used (or booted enabled).
Its measured stack margin and combined TLS/display load need hardware checks.
After first use the idle worker remains allocated until reboot.

## Checks

Run the `knx_mcp` host test, `node --test tests/webui/xiaozhi.test.mjs`, the
full web test glob, `python tools/check_web.py`, the font/catalog checks and a
`panel10` build.
Hardware acceptance must cover off-by-default privacy, pairing, hold/release,
speaker reply, disable during pairing/listening, reconnect and screen stability.
Do not claim hardware verification from builds or screenshots alone.

The simulator target `betta-xiaozhi-test` runs the production overlay against
LVGL through 100 enabled/disabled dashboard rebuilds and screen deletion. Run
it with `ctest --test-dir build/simulator/paneljc -R xiaozhi-view-lifecycle`.
It checks timer cleanup and manual versus voice-opened controls at five
geometries, including press/release/cancel and manual reopening after a voice
timeout. It does not contact a cloud service or open audio.

On 2026-10-26, the manual-open regression reproduced the hidden Talk button in
the old view and passed with per-view origin tracking. It covers wake readiness
changes, a later wake while manually open, both emoji styles, 100 rebuilds and
five geometries. All 337 browser tests, web integrity and strict docs passed.
The `panel10` build from `45ef89dd-dirty` succeeded (6,729,040 bytes; ELF SHA256
`6368e9c87cdd69325c63a3a89fd39e008b85bef0c72b962281706da2976e349c`).
That initial local image was retained; the following device trial used a
fresh build of the same source changes.

The authorized COM24 follow-up on 2026-10-26 ran `45ef89dd-dirty`, app SHA256
`1c0d98a59ca24252fa7f1312ef2745a4ca45a5df61ebe6480bbc715d92b695cd`, ELF SHA256
`fbb8522cf9504e7de0492a14c58b9b3f8a1772ef670cbb46bdff977b7e304442`.
The app-only P4 stub flash and readback passed; metadata/NVS, coredump and the
complete LittleFS checksums matched before boot. Boot ELF, source stamp and
valid OTA state matched. The header-opened window displayed Hold to talk with
Ready and no cloud capture despite an enabled, ready wake detector. Manual
press/release produced recognition and a reply; a later detected wake opened
the window without Talk and resumed listening after its reply. Device snapshots
and API transitions agreed. The owner confirmed both paths, replies, idle
auto-close and Talk returning on a subsequent header open.

The final read-only check at 511 seconds found no unexpected reboot or new
crash. Settings, layout, palettes, all 30 KNX objects and Chinese font metadata
matched the baseline except an owner-confirmed change to the digital clock
screensaver, which was retained. English, Classic Emoji, 180-degree rotation,
volume 60, active brightness 75 and the Classic palette remained set. Existing
local backups were preserved. This bounded functional trial does not resolve
the previously reported intermittent full-screen white flashes.

The header-state follow-up on 2026-10-26 built `panel10` from `45ef89dd-dirty`
(6,729,296 bytes), with ELF SHA256
`298677784b29a2365b4386583e9a7abe12e70a9327f7193faa38aa40ba11fa41`.
The five-geometry LVGL lifecycle regression and strict docs passed. The authorized
COM24 app-only stub flash passed readback and all protected-region checksums.
Boot ELF, source/build stamp and valid OTA state matched. Device screenshots
showed the active palette while manually open; another header click, Close and
the unchanged 30-second timeout each restored identical idle icon pixels.
A detected wake showed the same highlight without Talk, recognized speech and
returned a Chinese reply. Auto-close restored the idle icon. The owner confirmed
both opening paths, highlighting/restoration, replies, touch and normal output.
At 333 seconds, boot sequence remained unchanged and there was no crash report.
Settings, layout, themes, KNX configuration and Chinese font metadata matched
the baseline, including English, Classic Emoji, 180-degree rotation and the
digital clock screensaver. Local backups remain retained. This bounded trial
does not resolve the earlier intermittent full-screen white flashes.

Local validation on 2026-10-23: 299 browser tests passed (including eight
assistant cases), web integrity and strict documentation builds passed, and
`panel10` compiled with ESP-IDF 6.1. The synthetic editor was checked at desktop
and phone widths, including enabled controls, pairing status and reset. These
checks did not contact the cloud or operate a microphone. No firmware was
flashed in that initial check. Existing device settings and local backups were untouched.

On 2026-10-24, COM24 crash reports from ELF `70f4464c0` identified a stale
overlay timer accessing deleted controls after the dashboard screen was cleaned.
This happened before HA client startup, even with the assistant disabled.
The child-root ownership fix passed 100 real-LVGL rebuilds and screen deletion,
then a `panel10` build and app-only stub flash (ELF `329ee66bf`). The existing HA
credentials authenticated; three layout saves and Chinese language switches
completed without a new crash. The owner confirmed stable output and touch.
Protected storage checksums matched across flashing; English was retained.
This verifies disabled-overlay lifetime, not voice/cloud functionality.

## Local wake-up and emotions

Reference: [feat/knx_ip revision 9cb04bc9](https://github.com/betamoojw/xiaozhi-esp32/tree/9cb04bc9fd2f441af5952793d8fdd00581bb2000),
`main/audio/wake_words/esp_wake_word.cc`, `main/application.cc` and
`main/display/lcd_display.cc`. This integration follows direct WakeNet detection,
auto listening mode and the `llm.emotion` protocol field, using this project's
single-worker audio ownership and local theme/font system.

`xiaozhi_wake.c` owns WakeNet and VAD. The 16 kHz ES7210 input supplies MIC1 to
the mono detector; the second slot is playback reference. Idle samples feed only the local detector. During a cloud session the
same worker owns Opus, playback and VAD activity tracking; detection pauses, so
speaker output cannot immediately trigger another wake. Disconnect releases the
detector, which is recreated after a short rearm delay. Failed initialization retains manual
Talk and retries after 30 seconds. Disable/reset closes audio and frees the
model instance; no wake task or model is created while the option is off.
The idle capture stream closes before a cloud handshake and restarts at
`listen/start`, keeping queued wake audio out of the next speech capture.

`tools/pack_xiaozhi_model.py` packs `wn9_nihaoxiaozhi` from the pinned dependency
into application rodata. `srmodel_load` reads that embedded data; no model
partition, LittleFS migration or separate model flash is required. OTA carries
the matching model automatically. Espressif's dependency supplies model and
inference licensing. Other boards retain unsupported audio status; the common
UI and protocol settings do not imply microphone-driver support on those boards.

NVS adds `wake` (default false) and `dialog_timeout` (default 30 seconds) under
the existing Xiaozhi namespace. The guarded API accepts bounded `wake` and
`timeout` commands. The worker publishes dialog/activity sequence counters;
only the UI timer changes LVGL or wakes the display. Neither polling nor
silent microphone frames extend the timeout. Text remains separated as STT
and latest TTS sentence, with the original `text` API field retained. Unknown
emotion names use neutral. Classic Emoji use the bundled monochrome Noto outlines,
with hero-role sizing bounded by popup geometry; Otto Robot is the default.

## Otto Robot assets

The 21 animations come from MIT-licensed
[`txp666/otto-emoji-gif-component` 1.4.0](https://components.espressif.com/components/txp666/otto-emoji-gif-component/versions/1.4.0/readme),
revision `4279001383c2b1a39ed0457b14759e7c87537941`. Attribution and the
original license are retained in `main/ui/assets/otto/`. The preparation tool
reduces frames to 96 × 96 while retaining timing and expression sequences:
387,465 bytes total. The manifest records source and output hashes. Builds need
only Python's standard library to embed and verify these maintained GIFs.
Pillow is needed only to regenerate them from the pinned upstream archive.

Panel10 enables LVGL GIF decoding and embeds the images in application rodata;
the unchanged OTA partition carries them without a filesystem migration.
The guarded `/api/xiaozhi/emoji?emotion=happy` route serves the same bytes to
the browser, with unknown names mapped to neutral. Other boards keep their
unsupported assistant behavior and do not include these assets.

NVS key `xiaozhi/emoji` stores 0 (Otto) or 1 (Classic); an absent/invalid key
defaults to Otto. Web `emoji` commands and deferred native System Settings saves
call the same bounded setter outside the display lock. An emoji-only save does
not rewrite runtime settings or allocate the audio worker. Reset returns to Otto;
disable/re-enable retains the choice. Seven built-in languages name both styles.

The UI creates one decoder when the dialogue is visible and Otto is selected.
Changing emotion replaces its source; unchanged polling does not restart it.
Close, timeout, disable and Classic selection delete the decoder and its timer.
Decode failure falls back to Classic. Animation frames do not extend the voice
idle timeout. The popup palette, text areas and state/pairing labels are shared.

Local checks on 2026-10-25: 321 browser tests passed, including saved wake/timeout
controls, disable/re-enable, reset and manual fallback. Real-LVGL checks passed
100 view rebuilds plus wake-open, activity renewal, timeout, close, theme color
and bounds at five geometries. System Settings/alert regressions also passed.
Real-font previews covered 480-square, landscape and portrait, all four styles,
large text and Chinese labels. The synthetic editor passed 1280- and 390-pixel
width checks, including control-group bounds, wake/manual visibility and a typed
timeout surviving reload. Web integrity, catalogs, fonts and strict docs passed.

ESP-IDF 6.1 built `panel10` from `b2b399c6-dirty`: app 6,233,360 bytes,
582,384 bytes free in the unchanged OTA slot. App SHA256
`e29b07b2f965626feb5ff681d3a69a7e91b9843799dda9e7c1b4aac04d51c9f1`,
ELF SHA256 `f511f598d14ac3139c30df7f9ffb7cf65d3fe09e2cd8304390959051a57c87d1`.
The 290,551-byte embedded model passed offset/bounds and original-payload checks;
unrelated dependency versions remain locked. This is local validation only:
no COM24 flash, microphone/cloud session or device-settings write was performed.
Existing settings, English, rotation and local backups were untouched.

Otto extension checks on 2026-10-25: all 322 browser tests passed. The two
affected real-LVGL regressions passed, including all 21 GIF decoders/frame
advancement, switching styles, timeout/timer cleanup, 100 rebuilds and five
geometries. Native settings exercised Save, Cancel, failure/retry and reopen.
Asset hashes, dimensions, frame counts and complete loop durations match the
manifest and pinned originals. A synthetic editor exercised immediate selection
and reload at desktop and 390-pixel viewport widths; controls stayed within
their group. These checks used no microphone, cloud service or physical panel.

ESP-IDF 6.1 built the Otto extension for `panel10` from `b2b399c6-dirty`:
app 6,634,960 bytes, 180,784 bytes free in the unchanged OTA slot. App SHA256
`1b3ef5f6a407eaf377aae8470f6d18577f865c0b6db9fa048e0cbf9bc9f9e439`,
ELF SHA256 `59076286c6b598c3eba6a1e65ca28ce16d67e6539f37193f29862cbde0661455`.
That build was locally validated before the following device trial.

## Otto COM24 trial

On 2026-10-25 the owner authorized flashing the above Otto image to the
Waveshare LCD-X ESP32-P4 v3.2 on COM24. The supported flasher stub backed up the
previous app slot, wrote only `ota_0`, and verified the app digest. Metadata/NVS,
coredump and the complete 18.625 MiB LittleFS region were byte-identical before
and after the write, before boot. The boot ELF prefix `59076286c`, running source
and timestamp matched, OTA was valid, and Home Assistant authenticated.

The panel served all 21 GIFs with the maintained asset hashes. Web selection of
Classic survived reload and appeared in the native dialogue and System Settings.
Selecting Otto on the panel remained a draft until Save; afterward the API and
web control both reported Otto. English and 180-degree rotation remained set.
The owner also exercised manual voice: the device reported recognized text and
a cloud reply while the monitor observed listening/speaking/ready transitions.
The first hands-free trial failed: the owner reported no response to the wake
phrase. Disabling wake then crashed at `WakeNet9 model_clean`, captured with the
matching ELF. Disassembly of ESP-SR 2.4.7's P4 library shows `clean` walking nine
layer pointers while create/destroy allocate/free six; the fault was at index 6.
The wrapper now releases/recreates the detector without calling `clean`, checks
its mono format, and selects MIC1 instead of averaging microphone and playback
reference. A host test covers repeated rearm, partial initialization failures,
format rejection and reference exclusion. API counters expose frames, hits,
input peaks and inference time without retaining audio. These diagnostics do
not by themselves establish acoustic wake recognition.

The corrective `panel10` image was built from `dev` at `b2b399c6-dirty`,
with runtime build stamp `Oct 25 2026 08:20:59`. It is 6,635,424 bytes,
with 180,320 bytes free in the existing OTA slot;
ELF SHA256 `e6b1c370a6a0518fbdaed2ece5d60e6e278693afcd1602131ed9bc88fe8b28ff`,
app SHA256 `f167e354a005251b4ec324620a99a60078b77563ef9c71ac12d3057d66048675`.
The app-only stub reflash again preserved all protected storage digests.
Running source/build stamp, boot ELF, valid OTA and all 21 GIF hashes matched.
Four enable/capture/disable cycles passed without reboot or another report.
The observation then recorded five local wake hits, cloud recognition/replies
and changing emotion states. The owner confirmed hands-free wake, replies,
Otto animation, idle auto-close and a stable physical display. The regression
checks for native settings and the LVGL dialogue also passed after the fix.
This is a bounded COM24 trial, not an all-day acoustic or stability test.
The fixed-image monitor collected 225 samples over 491 seconds, with no poll
errors, unexpected reboot or new crash report. The original crash report and
all local firmware/data backups remain retained.
Final read-only comparison at 843 seconds preserved English, 180-degree
rotation, layout, themes, pairing and the original wake-off preference. The
owner confirmed Classic Emoji as the final choice. A read-only check at 954
seconds confirmed that choice, English, 180-degree rotation and unchanged
settings/layout/themes, with no new crash. Both preference snapshots were
retained locally.

The capture exposed a pre-existing font limitation: arbitrary Chinese cloud
responses can contain characters outside the compact built-in CJK subset and
show missing-glyph boxes on the LCD. The web transcript uses browser fonts and
shows the full response. The UI catalog font checks do not establish coverage
of unrestricted AI-generated text. The subsequent local transcript resource and
HTTP connection-budget correction are described in [Xiaozhi resources](xiaozhi-resources.md).
