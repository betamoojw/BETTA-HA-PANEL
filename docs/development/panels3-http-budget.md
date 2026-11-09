<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 HTTP memory budget

Scope: Guition ESP32-S3-4848S040 / `panels3`, 2026-11-08. This is a local
implementation, regression-test and build record following the
[COM18 discovery trial](panels3-entity-discovery-followup-validation.md).
No device HTTP requests, serial access, resets, flashing, settings/layout/history
writes, commit or push are part of this phase. The earlier physical acceptance
belongs to its recorded image, not the new unflashed firmware.

## Findings

The previous image demonstrated a 1,252-byte internal-memory decrease when
creating the shared HTTP client. After one successful query, a later query
repeatedly reported 36,255 bytes free: 609 bytes below the 36 KiB priority gate.
It timed out despite a suitable contiguous block. A later retry succeeded after
natural memory recovery. This is an admission/lifetime mismatch, not proof of a
leak or a complete accounting of concurrent heap activity.

The pinned ESP-IDF 6.1 source distinguishes `esp_http_client_close()` from
`esp_http_client_cleanup()`. Close shuts the transport; cleanup also destroys
headers, control objects, transport state and RX/TX buffers. The shared HA handle
previously remained allocated after successful discovery or an HTTP-budget timeout,
until a later transport error, endpoint change or HA stop. The two 2 KiB S3 buffers
prefer PSRAM; smaller retained objects still consume internal RAM.

The existing request checks already require 32 KiB free after setup and 24 KiB
while connected. The old admission reserve did not account for that guarded
execution path or reclaim an idle client while a query waited for memory.

## Client ownership and release

`ha_http_session.c` owns the shared handle and cached endpoint/certificate name.
Discovery, state sync and the immediate weather-forecast follow-up still use one
synchronous HA-task-owned client. There is no new worker or timer task.

On S3, the HA loop calls idle maintenance **after synchronous requests return**:

- The first idle boundary starts a monotonic two-second idle interval. Reusing
  the client clears that interval; a long request is not mistaken for idle time.
- At a later loop boundary, two seconds idle releases the whole client. Internal
  free memory below 36 KiB, or a largest block below 8 KiB, releases it immediately
  at that safe boundary, including while discovery is waiting for admission.
- A budget denial, cancelled generation, terminal timeout or successful page
  reaches the same maintenance path. Stop and endpoint changes still release
  explicitly. Cancellation/API callbacks never free a handle in active use.
- Short bursts may reuse the client. The idle interval is not a hard real-time
  release deadline: synchronous HA work must return before maintenance runs.

Release logs report reason, free internal memory before/after and the resulting
largest block. These remain samples affected by other tasks, not exact allocation
accounting. The P4 idle/pressure policy is a no-op, preserving its existing reuse.

The review also found a separate HTTPS lifetime defect: IDF's SSL transport
borrows the certificate-name pointer. The old shared-client initializer passed
a temporary caller-stack string. It now points into session-owned storage that
survives until cleanup. A regression test overwrites the caller's string and
checks the retained name. This applies to S3 and P4; it is **not** the cause of
the previous COM18 plain-HTTP memory waits. Certificate bundle/name validation
remains enabled as configured.

## Admission and protected margins

| Stage | S3 free internal memory | Largest internal block |
| --- | --- | --- |
| Initial query admission | 40 KiB | 8 KiB |
| After eight seconds, configured `ws://` → HTTP | 34 KiB | 8 KiB |
| After eight seconds, HTTPS or unrecognized scheme | 36 KiB | 8 KiB |
| Client, body and headers prepared | 32 KiB | 4 KiB |
| Connected, headers/read/parse boundaries | 24 KiB | 4 KiB |

The 34 KiB ordinary-HTTP gate is the 32 KiB prepared reserve plus a 2 KiB setup
allowance. The measured initialization delta was 1,252 bytes; header setup and
concurrent allocation remain subject to the prepared check. This allowance is
not a bound on all SDK allocations. If setup consumes the reserve, the request
does not connect and cleans temporary resources. The conservative HTTPS gate is
unchanged because the trial did not measure HTTPS costs.

At least 96 KiB free PSRAM and a 16 KiB PSRAM block remain required. Twelve-entity
pages, the 8 KiB response cap, adaptive page reduction, HTTP rate/cooldown budget,
WebSocket/sync priority, 45-second no-progress deadline, 180-second total deadline
and generation ownership are unchanged. A genuine shortage still waits and can
time out; neither polling nor client cleanup resets those deadlines.

The request now checks the connected reserve **after JSON parsing** too. cJSON
prefers PSRAM but can fall back to internal memory. If parsing drops below the
reserve, its result is freed and the request follows the existing bounded retry
path. The waiting log now prints the same fresh free-memory sample used for
admission, instead of an earlier loop snapshot.

P4 retains its 120/48 KiB initial and 64/24 KiB priority admission pairs,
40-entity pages, 16 KiB response cap and 1 KiB TX buffer. The S3 request-stage
guards remain disabled on P4; only the shared certificate-name lifetime repair
changes its client initialization ownership.

## Local regression coverage

- C policy passes for S3 and all five P4 profiles. Captured 35,003, 35,319 and
  36,255-byte heaps are admitted for ordinary HTTP after the grace interval;
  HTTPS still waits. Exact free/block/PSRAM boundaries and unknown schemes are
  checked, alongside timeout, retry, cancellation and stale generations.
- Shared-session C tests pass **92 checks on S3 and 62 on each P4 profile**.
  They cover burst reuse, completion-relative idle age, pressure release,
  endpoint/certificate replacement, persistent certificate-name storage,
  allocation failure/recovery, timeout/retry and cleanup after cancellation/stop.
  The 1,252-byte retained-client model replays measured pressure; it does not
  replace testing the real SDK allocator on hardware.
- Request and API C harnesses pass for S3/P4. All 26 existing injected request
  allocation-failure positions remain covered. A new parse-time internal-memory
  drop verifies result cleanup; prepared/connected/read guards and P4 behavior
  also pass. Network and allocator operations in these tests are mocked.
- All 25 editor discovery/lifecycle tests pass, including low-memory recovery,
  terminal timeout/Retry, cancellation order, keyword switching and picker
  close/reopen. Editor source was not changed in this phase.
- Six panels3 configuration checks, source-size checks and host-runner shell
  syntax pass. The first configuration invocation skipped without `CC`; it was
  rerun with the existing compiler. Windows blocked the first shell invocation;
  the authorized local rerun passed. Neither failed invocation is counted as a pass.

The full POSIX host suite and P4 firmware builds were not run. This phase does
not qualify physical display behavior, HTTPS peak use, real reclaimed bytes,
new stack low-water or sustained repeated-query success. A later authorized
device trial should compare release logs and query stages against the previous
COM18 results while preserving the same layout, history and configuration.

## Build and evidence

The panels3 build uses the existing ESP-IDF **v6.1** environment and project
PowerShell wrapper, with `reconfigure build` and compiler caching disabled for
an accurate runtime build stamp. No sdkconfig, board GPIO, display/touch profile,
Wi-Fi configuration, task stack, persisted clock setting or page definition was
edited. Build identity and final checks are recorded with the finished artifact.

| Final application | Value |
| --- | --- |
| Source / runtime stamp | `cef3fce1-dirty` / 2026-11-08 21:21:39 |
| Application size | 4,494,112 bytes |
| OTA slot remaining | 2,321,632 bytes (34%) |
| App SHA-256 | `37036a4f7c9170979884787b193a4b3e99af0344b8e9065f807def6ce3b27de6` |
| ELF SHA-256 | `ca85dc49c347884907193e29678260396845c815cbfbe448ebcff25c443a7bd9` |

The generated sdkconfig, partition table and bootloader are byte-identical to
the last flashed archive. ELF type information shows the shared HA state grew
from 17,176 to 17,200 bytes: 24 bytes for lifecycle metadata/alignment, with no
new task stack. This is a compiled object-size comparison, not a heap measurement.

The first build succeeded but retained the old build-stamp object. The existing
CMake touch target uses ordering dependencies, which did not make Ninja recompile
that object in the same invocation. Touching `main/build_stamp.c` **before** the
final wrapper build, with compiler caching disabled, produced the timestamp above;
file content and the build system were not otherwise changed for this workaround.
The IDF application descriptor retains its older incremental-build timestamp, so
identify this image by the runtime stamp and hashes together. Existing managed-
component/include warnings, an unused cover-art helper and an LVGL deprecation
remain; no new compiler error occurred.

The evidence/whitespace/source-size checks and strict documentation build pass.
Four affected pages are checked over HTTP/file at wide/narrow sizes and two page
scales, including the previous trial's image zoom. Those captures remain dated
evidence from that earlier firmware, not captures of this unflashed update.

Private checks and build logs are under
`.local/validation/panels3-http-budget-2026-11-08/`.
The image, configuration, exact working-tree source overlay and checks are in
`.local/firmware/panels3-http-budget-cef3fce-dirty-2026-11-08/`, separate from the
previously flashed firmware. The source stamp alone cannot distinguish these
dirty working-tree images. No staging, commit or push was performed by this work;
an externally changed staging state was observed and preserved.

The subsequent [COM18 validation](panels3-http-budget-validation.md) records the
authorized application-only flash, nine successful queries, measured client
release, data preservation and final owner-confirmed flip saver. Its initial
clock-sync delay and unexercised failure paths remain explicit limitations.

## Pre-commit review

The 2026-11-08 review rechecked the frozen 21:21:39 image, protected-partition
checksums, unchanged layout, nine successful requests, release measurements and
final owner confirmations. The offline evidence audit passed before source
changes. The original failed/limited trials remain recorded separately; the
later success does not retroactively qualify them.

Three request-path findings were fixed after that device trial:

- IDF returns zero for chunked/unknown content length, but negative values for
  header-read failure. The request had treated both as unknown length. It now
  stops on header errors, even after a partial 200 status line, and maps IDF's
  header timeout to a retryable timeout without accepting the partial response.
- The five-second I/O budget was checked before reads only. A final blocking
  read could finish after the deadline and still succeed. Open, write, headers
  and each read now check elapsed time on return; late data is discarded. An
  already-blocked SDK call still follows its own timeout, so this is not a hard
  five-second wall-time guarantee.
- JSON failure cleanup could free nodes before recording the parse boundary's
  low memory. Sampling now precedes validation/cleanup. The new test reproduces
  memory recovery during cleanup and fails on the old sampling order. Logged
  values remain stage samples, not allocator peak measurements.

The shared request fixes apply to S3 and P4. Board admission values, PSRAM
policy, session reuse/idle release, stacks, display/touch configuration and
saved settings are unchanged. No additional lifecycle defect was found in the
ordered picker requests, conditional cancellation or HA-task-owned cleanup.

Local verification passed 16 C executables (32 compile/run steps): policy and
session tests for S3 plus all five P4 profiles, and request/API tests for S3/P4.
The request tests include negative header results, zero-length/chunked handling,
expiry after each blocking stage, parse-failure sampling and the existing 26
allocation-failure positions. All 552 editor tests and six configuration tests
passed, as did web consistency, translation assets, JavaScript/shell syntax,
source sizes, simulator-field/test-page consistency and gallery freshness.
The strict documentation build and 64 local HTTP/file, wide/narrow and page-scale
checks passed, including four image-zoom open/close checks across the records.
Windows temporary-directory access blocked the first web/config checks; their
authorized reruns passed. Those blocked attempts are not counted as successes.
The full POSIX host suite, rendering regressions and P4 firmware builds were
not rerun; the changed paths contain no LVGL/widget-rendering implementation.

The wrapper's ESP-IDF v6.1 panels3 `reconfigure build` passed. This **unflashed**
review image is separate from the hardware-qualified 21:21:39 image:

| Review artifact | Value |
| --- | --- |
| Source / runtime stamp | `cef3fce1-dirty` / 2026-11-08 22:19:48 |
| App size / OTA slot free | 4,494,352 / 2,321,392 bytes (34% free) |
| App SHA-256 | `6a19232e2480a84846c3fa0879c413ec49d07e5fc296b56f6b7b9959d7984d51` |
| ELF SHA-256 | `dbd2dd159cb128433b3f23ef0fb391df286608a51908080f53cc3254b9f6d565` |

Its sdkconfig, bootloader and partition table match the flashed image byte for
byte. HA state remains 17,200 bytes; no stack was added. The image/source archive
is `.local/firmware/panels3-entity-review-cef3fce-dirty-2026-11-08/`; review logs
are `.local/validation/panels3-entity-review-2026-11-08/`. No device request,
serial open, flash, reset or configuration write was made during this review.
That review's preservation baseline was Asia/Tokyo, 22:00 night start, flip saver
and 12-hour format. At that point the new edge-case fixes had local test/build
evidence only; the COM18 record's hardware limits still applied.

The later [ce2ffa7 device validation](panels3-ce2ffa7-validation.md) rebuilt the
committed fixes, flashed only COM18's application and recorded eight successful
queries, measured memory release and owner-confirmed flip-saver operation.
It records the owner's later America/New_York and Simplified Chinese choices
and separately lists the failure paths that were not induced on hardware.
