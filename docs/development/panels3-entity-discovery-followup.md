<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# S3 entity-search follow-up

Scope: Guition ESP32-S3-4848S040 / `panels3`, 2026-11-08. This record describes
the local implementation/build phase following the
[failed first discovery trial](panels3-entity-discovery-validation.md).
That phase made no device requests, flash, layout/settings/history writes,
commit or push. The subsequent authorized
[COM18 trial](panels3-entity-discovery-followup-validation.md) records successful
queries, lifecycle checks, low-memory timeouts and a successful retry after natural
recovery. Immediate repeated queries remain memory-limited; physical acceptance
is recorded there.

## Internal memory findings

The COM18 log places the largest-block drop **before entity discovery**:

| Captured stage | Internal free | Largest block |
| --- | --- | --- |
| Immediately before WebSocket start | 54,679 bytes | 30,720 bytes |
| WebSocket connected | 33,435 bytes | 8,704 bytes |
| HA authenticated | 34,959 bytes | 8,704 bytes |
| Later query samples, including a closed editor | 31,331–39,891 bytes | Always 8,704 bytes |

No template HTTP attempt occurred in that image: its 12 KiB priority block
floor could never admit this heap. The boot-wide 7,796-byte low-water occurred
before discovery and is not a measured query cost.

The pinned ESP-IDF 6.1 allocator prefers internal RAM for `malloc` sizes at or
below 1,024 bytes, then PSRAM for larger sizes, with fallback. The SDK's shared
HTTP client allocates RX/TX separately. S3 TX is now **2,048 bytes**, matching RX,
so both prefer PSRAM; the former 1,024-byte TX preferred internal RAM. This
removes a known avoidable allocation from the internal pool in the normal case.
It is an allocation-policy finding, not a measured 1 KiB hardware saving.

S3 ELF type sizes are: HTTP control objects 488 bytes in total, separate
transport objects 64/120 bytes, and `esp_tls` 2,916 bytes. These exclude linked
headers, allocator overhead and network traffic. TLS uses the existing external
mbedTLS allocator; its 16 KiB receive and 2 KiB send record limits do not imply
equally large internal blocks. Query text (4 KiB), bounded responses and large
discovery caches stay explicitly in PSRAM; cJSON retains its PSRAM-first hooks.
The host request fixture serializes a 3,020-byte body; that is not every HA reply
or the target's transient peak.

The existing 48 KiB DMA/internal reserve, Wi-Fi allocations, 14 KiB HTTP task,
12 KiB HA task and 8 KiB WebSocket task share the internal pool. The SDK creates
the WebSocket stack internally and keeps small control allocations internally.
Their lifetimes and allocation order can split available blocks. The log locates
the loss at startup/connection, but there is no allocator trace identifying every
allocation responsible for the 21,244-byte drop. No stack, Wi-Fi/LCD setting or
device configuration was changed to reclaim that memory.

## Admission and bounded requests

| S3 stage | Free internal reserve | Contiguous block |
| --- | --- | --- |
| Initial admission | 40 KiB | 8 KiB |
| Admission after eight seconds | 36 KiB | 8 KiB |
| HTTP client/body prepared, before connect | 32 KiB | 4 KiB |
| Connected, headers/read/parse boundaries | 24 KiB | 4 KiB |

The 8 KiB floor fits the observed 8.5 KiB heap without assuming a new task stack
or a TLS record in internal RAM. The total reserve is unchanged; 35,003 bytes
free still waits. Admission also requires 96 KiB free PSRAM and a 16 KiB block.
If preparation, connection or reading consumes the protected margin, the request
closes and releases temporary buffers, then retries within the existing deadline.
These are conservative stage budgets, not qualified peak-memory limits.

Twelve-entity pages, an 8 KiB response cap, adaptive page halving at the same
offset, the shared HTTP rate budget and WebSocket/sync/queue gates remain.
No progress for 45 seconds, or 180 seconds overall, terminates work. Retrying
does not discard a ready cache. P4 retains its 120/48 and 64/24 KiB admission
pairs, 40-entity pages, 16 KiB response cap and 1,024-byte TX buffer. The new
S3 request-stage restrictions do not apply to P4.

`HTTP client memory` now records free/largest values even on initialization
failure. `Page memory` includes the failure stage, result, body/response lengths
and minimum **sampled** free/largest values across request stages. Samples include
concurrent task activity and can miss intermediate peaks. The subsequent trial
measures two HTTP requests and stack headroom; TLS remains unqualified.

## Interleaved-query findings

The owner confirmed no other entity searches during the 18:41 device test.
The device trace has no browser/context identifier, so it cannot establish which
window produced each request. The original in-app-browser sequence is not fully
reconstructable from serial evidence alone.

A loopback browser replay uses the archived flashed JavaScript, the production
HTML and **all `bindUi()` event bindings**, with a simulated API. Invoking the
direct Button shortcut while the general add-widget search remains pending
reproduces `button`, empty search, `b`, then repeated `button`/`light` alternation.
The Button field picker uses `domain=any` and permits short searches; the general
add-widget search requires two characters. Thus the empty/one-letter requests
are consistent with overlapping picker activity, not proof that its two-character
guard failed. This is a demonstrated local mechanism, **not proof that the same
shortcut occurred in the historical hardware test**.

Confirmed fixes:

- Opening either picker stops the other; reopening a field picker invalidates
  old polls and replies even when the new picker only shows a cached/idle state.
- GET and DELETE share a bounded queue. Closing during an in-flight first GET
  waits for its response before cancelling the returned generation; a replacement
  query waits behind that cancellation. Unknown first-request outcomes expire on
  the device rather than cancelling unowned work with the same keyword.
- Polls and DELETEs carry the server's generation. A stale poll receives a
  terminal replacement reason and cannot restart its old keyword; stale
  cancellation cannot stop a newer generation. Malformed generations are rejected.
  Requests without a generation remain compatible with older clients.
- Keyword changes, Enter/Retry, close/reopen and page departure invalidate
  obsolete callbacks. Existing after-sleep sequence checks are retained.

The current browser replay stops the overlap. A separate two-context replay
stops the older query with a replacement/Retry message while the newer query
recovers from simulated low memory. Normal keyword changes and close/reopen
also pass. No request is sent to the real panel or Home Assistant by these tests.

## Local checks and image

- Complete editor suite: **552 passed, zero failed**. Focused follow-up checks
  cover the 25 discovery/lifecycle tests, including production input/Enter/Retry
  handlers, empty searches, delayed GETs, cancellation order and page departure.
- C policy: S3 and all five P4 profiles. Captured 8.5 KiB blocks, free/block
  boundaries, PSRAM limits, timeout/retry and stale generations pass.
- C HTTP/API: S3 and P4. Preparation/connection/read memory loss, recovery,
  cleanup, 26 injected allocation-failure positions, generation parsing/routing,
  malformed requests and legacy compatibility pass. HTTP/allocator calls are
  mocked; these are not TLS hardware measurements.
- Six existing S3 build-configuration tests, source/bundle/catalog checks and
  host-runner syntax pass. The full POSIX host suite was not run on Windows.
- Full-binding Chromium checks pass 12 width/theme/language combinations
  (1440/390 px, dark/light, English/Chinese/German), with control bounds, messages,
  Retry, close and retained synthetic layout. Lifecycle checks additionally compare
  the archived/current implementations and two independent browser contexts.
- Strict documentation build and five affected pages over HTTP/file, at wide
  and narrow sizes and two page scales, pass. These 40 cases check local links,
  images and bounds; the first-trial screenshot remains a dated device capture.

The `panels3` wrapper build uses ESP-IDF **v6.1**, source **`cef3fce1-dirty`**.
The final build disables compiler caching for an accurate runtime timestamp.
The generated sdkconfig, partition table and bootloader match the flashed archive.
Known SDK/component warnings and the unrelated unused cover-art function/LVGL
deprecation remain; no new compiler failure occurred.

| Final artifact | Result |
| --- | --- |
| Runtime build stamp | 2026-11-08 19:25:59 |
| Application size | 4,493,616 bytes |
| Application slot remaining | 2,322,128 bytes (34%) |
| App SHA-256 | `980a884588e3f635e4392a877368d9d04319f614c7c3e2eb5f05a1373ea702b9` |
| ELF SHA-256 | `b7bb0a79bba0759b2decf1ef6cae0f6d816283feae633bfee116f4c4656f97b8` |

The prepared image archive is
`.local/firmware/panels3-entity-discovery-followup-cef3fce-dirty-2026-11-08/`.
It includes the exact source overlay, configuration and artifact hashes; a dirty
source stamp alone cannot distinguish this image from the first trial's image.

Private test logs, simulated browser traces, hashes and build logs are in
`.local/validation/panels3-entity-discovery-followup-2026-11-08/`.
The later COM18 results, remaining memory limitation and data-preservation
evidence are in the linked trial record. Pages, graph history, device
configuration, flip style and 12-hour format were not changed or re-saved.
