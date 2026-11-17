<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 authentication and PNG memory

Guition ESP32-S3-4848S040 / `panels3`, local fixes and validation on 2026-11-17,
based on `447fbf7d`. This follows the [COM18 device trial](panels3-447fbf7-validation.md)
at `192.168.71.77`. **This was a local-only phase.** No device HTTP/serial
access, reset, layout/settings/history write, commit or push occurred in this phase.
The subsequent [COM18 validation](panels3-auth-png-validation.md) records the
authorized flash of this exact image and its separate hardware results.

## Findings and fixes

The previous trial repeatedly stopped before HTTP-client preparation: typical
free internal memory was about 32 KiB, below the 34 KiB plain-HTTP admission
reserve. Largest blocks of 12–17 KiB already exceeded the 8 KiB requirement.
User management added 3,522 bytes of static account, session and login-attempt
tables. This is a measured contributor to pressure, not a full fragmentation
trace or an explanation for every runtime free-heap change.

S3 now keeps those HTTP-task-owned records in bounded PSRAM pools, allocated
on first use and retained for the server lifetime. The compiled account/session
pool is 3,144 bytes; sixteen login-attempt records add 384 bytes. Temporary NVS
transaction copies also use PSRAM and are wiped before release. Failure never
falls back to internal RAM or open access: protected routes return 503, and
later requests can retry allocation. A failed login-table allocation never
skips throttling. NVS record layout, credentials, roles, sixteen-user/session
limits and expiry remain unchanged. Async leases only read internal atomics.

The final S3 ELF reduces `.dram0.bss` from 98,104 to 94,584 bytes: **3,520 bytes
reclaimed**, including linker alignment. `.dram0.data` and IRAM sizes are unchanged.
This is static allocation evidence, **not** a measured increase in running free
heap. The permanent PSRAM payload is 3,528 bytes, excluding allocator overhead.
P4 keeps its static internal account/session/attempt storage.

The shared HTTP client still has one HA-task owner. Review of the pinned IDF
6.1 source and the caller confirms full `esp_http_client_cleanup()` after I/O,
not just socket close, and no handle release from the browser cancellation path.
The prior trial never created a client, so it supplied no new client-cost or
release measurements. Its failure is not evidence of an HTTP-client leak.
Existing two-second idle release, immediate low-memory release, 2 KiB RX/TX
buffers and guarded preparation remain. No extra task or polling allocation
was introduced; see [S3 HTTP memory budget](panels3-http-budget.md).

| Preserved S3 query guard | Free internal / largest block |
| --- | --- |
| Initial admission | 40 / 8 KiB |
| Ordinary HTTP after eight seconds | 34 / 8 KiB |
| HTTPS after eight seconds | 36 / 8 KiB |
| Prepared client/body/headers | 32 / 4 KiB |
| Connected/read/parse boundaries | 24 / 4 KiB |

The 96 KiB free / 16 KiB largest PSRAM requirement, twelve-entity pages,
45-second stalled / 180-second total deadlines and generation ownership also
remain. The recovered static RAM targets the observed plain-HTTP shortfall;
it does not guarantee success during other concurrent allocations or TLS use.
Genuine shortages must still wait, time out and allow explicit retry.

Review also found that returning to the sign-in screen did not explicitly stop
entity pickers. It now closes both pickers, invalidates old poll callbacks and
queues generation-qualified cancellation. A failed unauthenticated cancellation
cannot bypass the guard; the firmware deadline remains the fallback. Reopening
after sign-in starts a new search. This fixes a browser lifecycle gap, not a claim
that it caused the earlier intentional `button`/`light` sequence.

The failed PNG path reserved a contiguous 1 MiB output before capture. It now
keeps full-resolution capture bands, encodes into bounded 8 KiB output blocks,
and uses a 4 KiB compression buffer plus one RGB row. All PNG work buffers are
PSRAM-only. Multiple IDAT chunks contain one continuous zlib stream, with CRCs
and the existing 1 MiB total encoded limit. Encoding finishes before any image
bytes are sent; failures release partial output, zlib state and capture bands.
HTTP send failures also release every block. BGR-to-RGB conversion and the
shared base/top-layer composition remain unchanged. JPEG/BMP retain their paths.

## Local checks

Sixteen native C executables passed with `-Wall -Wextra -Werror` for project code:

- Account store, HTTP account guard and license HTTP integration, each in S3/P4
  modes: unavailable PSRAM, recovery, no credential changes on failure, bounded
  session reuse, wiped transaction copies, protected screenshot/discovery/cancel
  routes, cookie/CSRF checks, roles, revocation and login throttling.
- Discovery policy, shared session, request and API harnesses, each in S3/P4
  modes: admission boundaries, prepared/connected/parse guards, low-memory
  recovery, timeout, cancellation, stale generations, retries and allocation failures.
  The S3 shared-session suite passes 212 checks, including twelve sequential
  query/cancel/retry/release cycles; P4 passes 62 with unchanged retention policy.
  Its retained-client allocation double is not a new SDK measurement.
- Screenshot handlers with real zlib and simulated LVGL/HTTP/JPEG: 243 cases
  without JPEG and 308 with JPEG. All decoded PNG/BMP pixels and PNG chunk CRCs
  match the expected base/popup/opaque-top composition, including 480 × 480,
  landscape/portrait P4 and odd dimensions. Every application/zlib allocation
  ordinal, output exhaustion, render/lock failures and HTTP chunk failures clean
  up, including failure of the terminating chunk. A later capture succeeds.

The native 480 × 480 PNG model passes with only 900 KiB total and a 64 KiB
largest allocation. Payload peaks exclude allocator/real LVGL/other-task costs:

| Synthetic content | Peak allocated bytes | Largest allocation | PNG bytes |
| --- | ---: | ---: | ---: |
| Base pattern | 760,049 | 61,440 | 22,584 |
| Alpha popup | 799,648 | 61,440 | 59,195 |
| Opaque top layer | 752,640 | 61,440 | 4,234 |

Incompressible content separately fails cleanly at that total budget and
succeeds after restoring headroom. The test does not promise that every real
screen fits in 900 KiB or that actual UI rendering has no temporary allocations.

All **624 editor tests**, `tools/check_web.py`, six S3 configuration tests and
source-size checks passed. The 29 discovery/lifecycle tests include real editor
transport wrapping with simulated HTTP replies: session credentials, mutation
markers, successive searches, memory recovery, timeout/retry, keyword switching,
close/reopen and invalidation when sign-in replaces the editor. Layout fixtures
remain unchanged. Node tests do not establish actual browser cookie behavior.

Strict Zensical, documentation-size and host-runner shell-syntax checks passed.
The six affected pages loaded over localhost HTTP at 1280- and 390-pixel browser
viewports without document overflow; the existing 480 × 480 capture retained
its square aspect ratio and its image viewer opened/closed. The browser blocked
`file://` navigation, so offline rendering/search was not requalified. Initial
restricted-host uv cache and Bash process-object failures were rerun with normal
tool access; those failed invocations are not counted as passes.

## Finished build

| Item | Evidence |
| --- | --- |
| Build entry point | `scripts/build.ps1 panels3 reconfigure build` |
| Toolchain | ESP-IDF `v6.1` |
| Source / runtime stamp | `447fbf7d-dirty` / `Nov 17 2026 21:35:11` |
| Application size | 4,581,200 bytes |
| Space remaining in 6,815,744-byte slot | 2,234,544 bytes |

App SHA256:
`2577e02fa9e8b22d8bd3d25d93b80ac1edcb706d33581935ad787a67d6288821`.
ELF SHA256:
`c9d4543f5bf73e00447a4e5c8ea74f930eb3ce42329cd3ba0d13159a29788082`.

The sdkconfig, defaults, dependency lock, partition table and bootloader are
byte-identical to the preceding flashed archive. HTTP still has a 14 KiB stack;
the board, Wi-Fi IRAM and PSRAM configuration are unchanged. Existing SDK
component/include and Kconfig notices remain; modified application sources
compiled without new warnings. The app descriptor's ELF hash matches the archive.

Build/source hashes, static-memory comparison and test logs are retained under
ignored `.local/validation/panels3-auth-png-memory-2026-11-17/`; finished artifacts
and the source manifest are in `.local/firmware/panels3-auth-png-memory-2026-11-17/`.
These contain no new device backups, credentials or cookies.

## Separate hardware acceptance

The installed `447fbf7d` failures and its owner's display/touch confirmation
remain historical evidence. They do not qualify this new build. The separate
[authorized COM18 trial](panels3-auth-png-validation.md) records consecutive
authenticated results, actual client setup/release costs, memory margins and
native PNG output for dark/light flip overlays. It also records the owner's
touch/wake confirmation and retained layout/settings, with remaining gaps explicit.

No current stack low-water, fragmentation trace, TLS peak, real screenshot
timing, P4 firmware build/hardware, Linux ASan/UBSan or complete POSIX host suite
was obtained. Shared rendering was not changed; real-LVGL simulator screenshots
were not rerun. Existing pages, graph history, language, time zone, clock format,
night schedule, flip screensaver and enabled login protection were not accessed
or altered in this phase. Populated graph history was absent from the preceding
trial's layout and remains unqualified.

## Pre-commit review

The 2026-11-17 review checked HTTP-task ownership of the PSRAM pools, fail-closed
allocation handling, cookie/Basic authorization, HA-owner-only client cleanup,
unchanged discovery reserves and PNG cleanup before and after transmission.
No further firmware implementation defect was identified. Additional S3 HTTP
cases verify that unavailable account storage rejects query cancellation and
that unavailable rate-limit storage rejects Basic authentication before hashing
or entering a protected handler; recovery still permits the normal login flow.

All sixteen native C executables were rebuilt and passed in S3/P4 modes,
including 243 screenshot cases without JPEG and 308 with JPEG. All 624 editor
tests, six S3 configuration checks, web-source consistency and source-size
checks passed again. This remains native Windows/mock coverage, not a complete
POSIX or sanitizer run. Test logs are retained under ignored
`.local/validation/panels3-auth-png-review-2026-11-17/`.

The review corrected two stale unflashed-build references and the device
record's overly broad exclusion of low-memory recovery: its raw log contains
one transient dip below 34 KiB followed by recovery and a successful query.
Sustained-pressure timeout/retry remains unqualified on the device. All 839
archived firmware input hashes still match, so the existing 21:35:11 build
remains the applicable firmware evidence; this review required no rebuild.
Only tests/documentation were adjusted. No device HTTP/serial access, reset,
flash, configuration or data write occurred during the review.
