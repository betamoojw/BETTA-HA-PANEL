<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 HTTP budget validation

Guition ESP32-S3-4848S040, COM18 / `192.168.71.76`, 2026-11-08.
The owner authorized application-only installation of the finished
[HTTP budget update](panels3-http-budget.md), query checks and physical
screensaver acceptance. The existing image was used without rebuilding.
No test widget, layout, HA action, commit or push was issued.

**The bounded discovery trial passed:** nine real HTTP requests completed,
including four consecutive queries and an explicit same-keyword refresh. The
earlier 45-second low-memory timeout did not recur. Keyword changes, picker
close/reopen and conditional cancellation passed. This is not a long-duration
or worst-case-memory qualification.

Network time initially failed to synchronize, and the flip saver correctly
showed placeholders. Time later synchronized; the owner confirmed normal clock
display and touch wake. During the trial the owner changed the timezone from
Asia/Shanghai to Asia/Tokyo and explicitly requested that it be retained.

## Image and application-only flash

COM18's ESP32-S3 revision 0.2 and MAC matched the identified HTTP device.
The previous application matched the archived 19:25:59 image. Its exact bytes,
NVS/boot metadata, crash partition and LittleFS were archived before writing.
Only active `ota_1` at `0x6A0000` was updated. Device checksums for metadata,
crash storage, LittleFS and the complete inactive app slot matched before and
after flashing, before normal boot resumed.

| Identity | Verified value |
| --- | --- |
| Project / version | `betta-ha-panel-s3` / `v0.9.2-rc1` |
| Source / ESP-IDF | `cef3fce1-dirty` / `v6.1` |
| Runtime and editor build stamp | 2026-11-08 21:21:39 |
| Application size | 4,494,112 bytes |
| App SHA-256 | `37036a4f7c9170979884787b193a4b3e99af0344b8e9065f807def6ce3b27de6` |
| ELF SHA-256 | `ca85dc49c347884907193e29678260396845c815cbfbe448ebcff25c443a7bd9` |

The boot log contains the matching ELF prefix. The IDF application descriptor
still carries its earlier incremental-build timestamp, **16:27:36**; hashes and
the runtime/editor stamp distinguish this image from other dirty builds.
The bootloader and partition table were not reflashed.

Boot sequence advanced 113 → 114. HTTP started, HA authenticated, touch
initialized, and the image remained `valid` without safe mode or a retained
crash. Replacing the already-valid active slot in place does **not** arm a new
OTA pending-verification cycle; this trial does not qualify OTA rollback.
Serial logging reused the flasher's open handle rather than reopening COM18.
The 1,067.8-second capture retains transient LVGL-lock failures, HA energy-
preferences warnings and the initial clock-sync timeout. They are not omitted
from the evidence; the UI watchdog recorded no restart.
After serial closed, uptime was 1,132 seconds in the same boot 114, with HA
authenticated, valid firmware, synchronized time and no retained crash.
After the later single-query follow-up, uptime was 1,381 seconds in the same
healthy boot. That follow-up used HTTP logs; COM18 was not reopened.

## Query and lifecycle results

One fresh in-app-browser editor was used with **Panel follows unchecked**.
Existing owner browser tabs were left untouched. The initial Energy page does
not support adding widgets; the test selected the existing LED page only in
the editor. No widget result was selected and Save was never used.
The separate API checks started after the test tab had been closed. The memory
observer never queried entities.

| Check | Observed result |
| --- | --- |
| General picker `button`, generation 1 | Initial internal-memory wait, then four matches appeared in the editor. |
| Change to `light`, generation 2 | Twelve matches appeared; the old keyword did not restart. |
| Pending `button` → `light` | Generation 3 was cancelled before generation 5 started. |
| Close pending picker, then reopen | Generation 5 was cancelled; the reopened picker had an empty search. |
| Reopened `button` search and Enter | Generation 7 completed with four matches. Closing it left no stale poller. |
| Stale generation checks | Old generation 8 could neither restart its query nor cancel newer generation 10. Matching cancellation stopped 10; its later poll was terminal `replaced`. |
| Four sequential queries | Generations 12–15, `button/light/button/light`, all succeeded with 4/12/4/12 matches in 8.996/9.417/9.259/9.084 seconds. |
| Explicit same-keyword refresh | `light`, `refresh=1`, started generation 16 and returned twelve matches in 8.912 seconds. |
| Follow-up with synchronized time and retained night policy | Generation 17 returned twelve `light` matches in 8.939 seconds, after serial/periodic observation had stopped. |

Every queued generation maps to a deliberate UI or protocol-test action. No
spontaneous `button`/`light` alternation appeared. The stale-generation test used
sequential requests representing two clients, not simultaneous browser users.

Initial waits below the unchanged 40 KiB admission floor remained visible.
For generation 15, the wait log started at 33,863 bytes free and recovered to
38,067 bytes before admission. No reset, forced heap exhaustion or device
configuration change was used to make a query succeed. The roughly eight-second
priority delay is expected, not an unresolved endless wait.

No query reached terminal timeout in this image. Therefore the real 45-second
timeout message and the UI Retry button **after a timeout** were not newly
exercised. The same-keyword refresh above proves the explicit new request path,
not recovery from an observed timeout. Those failure paths retain the earlier
host/web regression coverage and prior-image hardware evidence.

## Shared-client memory

The configured `ws` connection maps to plain HTTP. All nine request logs report
successful parsing. Each client was subsequently released at the HA-task boundary
for `internal_memory`; no release occurred inside an active synchronous request.

| Sampled measurement, nine requests | Bytes |
| --- | --- |
| HTTP-client initialization decrease | 1,240–1,284 |
| Release-time internal-memory increase | 1,660–1,692 |
| Minimum sampled internal free per request | 34,539–35,943 |
| Smallest sampled internal block during requests | 14,848 |

For the first request, initialization reduced free memory from 38,307 to 37,051
bytes. The request sampled a minimum of 35,611 bytes; subsequent cleanup changed
36,431 to 38,095 bytes. The release can recover more than initialization because
headers and request state are added later. Concurrent task activity also affects
these differences: they are not isolated allocation totals or peak traces.
The serial capture covers the first eight requests. The ninth is in its saved
HTTP log: initialization 37,531 → 36,287, sampled minimum 34,671 and release
35,339 → 36,999 bytes. It was run with synchronized time and the normal night
policy. The earlier generation 16 also has synchronized timestamps.

The connected-request guard remains 24 KiB free / 4 KiB contiguous; all measured
request boundaries exceeded it. These successful requests also passed the
32 KiB prepared guard. They do not establish the exact lowest safe admission
level. No persistent 8.5 KiB block occurred in this boot, and no request-stage
allocation failure was forced.

Compared with the [previous trial](panels3-entity-discovery-followup-validation.md),
the retained client now demonstrably releases memory, and consecutive queries
completed instead of the two observed low-total-memory timeouts. The layout and
background heap activity differ from that earlier trial, so this is a bounded
observational comparison, not a controlled benchmark or a guarantee that memory
waits can never time out.

Only pressure-triggered releases were observed. The two-second idle-only branch,
HTTPS costs and P4 hardware behavior retain local coverage, not new device
qualification. The S3 trial does not qualify other boards.

The five-second observer collected 198 samples at uptimes 43–1,061 seconds
without a collection error. Its wider workload included screenshots and runtime
settings saves: internal free ranged from 28,199 to 40,363 bytes. That range
must not be substituted for the request-stage samples above.
After the workload and BMP capture, least-ever free stack was HA 5,260/12,288
bytes, WebSocket 5,944/8,192, HTTP 7,348/14,336, LVGL 12,648/20,480 and UI
runtime 10,984/16,384. Boot-wide internal low-water stayed at 7,796 bytes;
that startup value is not the query peak.

## Saved data and flip screensaver

The baseline was captured immediately before this flash: **five pages and
thirteen widget definitions**, not the older trial's three-page layout. Its
3,186-byte JSON remained byte-identical, SHA-256
`aa13870324a9cb46ecabd7fba863e6675cff40afb4797508ee2c0eee304044d7`.
Theme and settings matched apart from live values and the explicitly
owner-confirmed Asia/Tokyo timezone change. The verification first reported
that difference; the original backup was preserved rather than silently replaced.

Flip style, 12-hour format, date/seconds display, 60-second saver delay, 15%
brightness and enabled 300-second screen-off remained unchanged. No settings
write was issued during the query tests. A later comparison detected night
start changing from 22:00 to 12:00, separately from the confirmed timezone edit.
Its origin was not established. At the owner's explicit request, one minimal
settings PUT restored **22:00**, with `reboot:false`; readback showed only that
field changed. Asia/Tokyo was retained. The serial log includes these runtime
settings writes, separate from the verified before-boot flash preservation.

LittleFS's flash checksum preserved any stored graph history. The current
layout contains no Graph widget, so populated graph-history rendering was not
exercised. Normal runtime/owner writes after boot are outside that checksum.

The first physical reply accepted the saver, but subsequent BMP/API evidence
showed `--` and `clock_valid=false`; the owner confirmed that actual state.
The original `0.wled.pool.ntp.org` server and 12-hour setting were retained.
All four addresses returned by a host-only DNS lookup answered an NTP probe;
this does not establish which address or network path the panel used.
Initial synchronization had timed out before the first entity query or HTTP
client cleanup. Its delay is not proven to be caused by this change.

Later readback reported `clock_valid=true`, and the owner confirmed correct
Tokyo time, clear weekday/date and AM/PM, centred flipping and normal touch
wake. The timezone change is not evidence that changing timezone fixed SNTP.
No separate time-setting request, NTP-server change or extra reset was issued.

The later BMP was captured after Tokyo time had entered the configured night
interval; `/api/version` reported `off` and it showed pixel refresh, not a clock.
The source's night-mode precedence explains that state. No night-policy override
was applied to obtain a clock capture. Thus the synchronized flip clock has
owner-reported physical acceptance, but no final daytime BMP in this trial.

## Evidence and limits

Private raw backups, before/after API captures, serial log, memory samples,
query responses, screenshot and owner confirmations are in
`.local/validation/panels3-http-budget-com18-2026-11-08/`.
The frozen application/source archive remains
`.local/firmware/panels3-http-budget-cef3fce-dirty-2026-11-08/`.
Its creation-time `flashed: false` describes the earlier build phase; this
trial's `flash-result.json` records the authorized installation.

No firmware source or tests were changed during this trial. The earlier C
S3/P4, 25 editor tests, six configuration tests and build results remain local
evidence. No P4 flash, forced network outage, artificial heap failure, prolonged
stress test, synthetic layout or graph-history reset was performed.

The evidence audit, documentation source-size and whitespace checks, strict
site build and 32 HTTP/file, wide/narrow, page-scale checks passed. These local
documentation checks do not expand the device qualification above.

The subsequent [pre-commit review](panels3-http-budget.md#pre-commit-review)
revalidated this evidence offline and corrected request-header error handling,
post-I/O deadline checks and parse-boundary memory sampling. Those fixes were
tested and built locally without accessing the panel. They are absent from the
21:21:39 image above; this trial must not be presented as their hardware test.
