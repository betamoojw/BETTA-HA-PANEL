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
  session. No MCP tools are advertised.
- `xiaozhi_audio.c`: LCD-X BSP pin definitions, ES8311 output and ES7210 stereo
  input; capture is mixed to mono at 16 kHz. Opus decodes server audio directly
  to 16 kHz. Audio opens lazily on conversation start.
- `xiaozhi_ui.c`: a UI timer reads a locked status snapshot. A child root owns
  the controls and timer; cleaning or deleting the dashboard destroys both and
  requests disconnect. The persistent screen must not own that cleanup callback:
  layout reloads clean its children without deleting the screen. Network tasks
  never call LVGL.
- `xiaozhi_api.c`: guarded GET/POST `/api/xiaozhi`, with bounded JSON commands.
  The API returns status, pairing code and latest transcript, never credentials.
- `components/webui/www/js/xiaozhi.js`: immediate-save controls, scoped reset,
  pointer/keyboard push-to-talk and cancellation on page exit.

Only `panel10` links `78/esp-opus` 1.0.5 and the audio implementation. Other
variants compile an unsupported implementation. The SDL path does not call
the hardware assistant.

Pairing uses HTTPS and the certificate bundle; WebSocket endpoints must use
WSS. A URL/token pair is committed in one NVS blob under `xiaozhi`; flash writes
use the shared OTA gate. The persisted UUID matches both activation and WS
headers. Pairing lasts at most 180 seconds and capture at most 60 seconds.
Pairing and listening begin only after explicit commands; enabling alone does
not contact the cloud. Disabling can wait for the current bounded network call.

Codec storage, PCM, receive frames and queued payloads use PSRAM. The worker
stack is 32 KiB internal RAM because it performs NVS writes and Opus encoding;
it is allocated only when the feature is first used (or booted enabled).
Its measured stack margin and combined TLS/display load need hardware checks.
After first use the idle worker remains allocated until reboot.

## Checks

Run `node --test tests/webui/xiaozhi.test.mjs`, the full web test glob,
`python tools/check_web.py`, the font/catalog checks and a `panel10` build.
Hardware acceptance must cover off-by-default privacy, pairing, hold/release,
speaker reply, disable during pairing/listening, reconnect and screen stability.
Do not claim hardware verification from builds or screenshots alone.

The simulator target `betta-xiaozhi-test` runs the production overlay against
LVGL through 100 enabled/disabled dashboard rebuilds and screen deletion. Run
it with `ctest --test-dir build/simulator/paneljc -R xiaozhi-view-lifecycle`.
It checks timer cleanup without contacting a cloud service or opening audio.

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
