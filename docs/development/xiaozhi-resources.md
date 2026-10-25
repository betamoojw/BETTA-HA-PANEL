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
