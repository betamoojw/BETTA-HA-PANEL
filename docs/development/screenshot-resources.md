<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Screenshot resources

Scope: the on-demand **Live view** toolbar action and `/api/screenshot.bmp`,
`.png` and `.jpg`, reviewed against `b48e186a` on 2026-11-14. **Live Display** is
the separate WebSocket mirror/remote-touch feature. This review does not qualify
either feature's performance on hardware.

## Panel10 memory model

For 1280 × 800, packed RGB888 needs **3,072,000 B** (2.930 MiB), while ARGB8888
needs **4,096,000 B** (3.906 MiB). Row strides are 3,840 and 5,120 B, with no
row padding in the current configuration. BMP uses a 54 B stack header and
streams rows, without a complete output allocation.

`api_screenshot_capture_locked()` captures an opaque full-screen top layer
directly as RGB. Otherwise it renders ARGB and scans alpha. A transparent ARGB
buffer is freed before rendering RGB. A visible popup retains ARGB while RGB
is rendered, blends into RGB, then frees ARGB. After unlocking, encoding keeps
RGB and the compressed output alive together. RGB is freed before sending;
the output is freed even if sending fails.

The popup's ARGB buffer therefore does **not** coexist with the encoder output.
These are incremental application buffer payloads, not whole-device heap peaks
or sufficient free-PSRAM thresholds:

| Path | Normal / transparent top | Partial popup | Opaque full-screen top |
| --- | ---: | ---: | ---: |
| Capture only / BMP | 4,096,000 B | 7,168,000 B | 3,072,000 B |
| JPEG, including 1 MiB output | 4,120,576 B | 7,168,000 B | 4,120,576 B |
| PNG, including row and zlib workspace | 4,163,021 B | 7,168,000 B | 4,163,021 B |

PNG uses a 1,048,576 B output, a 3,841 B row and zlib windowBits=12, memLevel=5.
Its four work arrays total 32,768 B; `deflate_state` is 5,836 B in the installed
P4/ESP-IDF 6.1 ABI, checked by compiling a size-only object. The Windows Host
ABI uses 5,920 B, giving 4,163,105 B instead. These are not hardware measurements.

Normal PNG peak: `max(ARGB, RGB + output + row + workspace)`.
Popup PNG peak: `max(ARGB + RGB, RGB + output + row + workspace)`.
JPEG omits the PNG row/workspace. JPEG output uses IDF's aligned PSRAM allocator;
PNG output/workspace explicitly request PSRAM. The PNG row requests ordinary
8-bit-capable memory, so its pool is not guaranteed. LVGL uses PSRAM first with
a general-heap fallback.

Add LVGL descriptors/alignment, allocator metadata, draw tasks, temporary layers,
image/font decoding and caches, plus JPEG engine, DMA descriptors, queues and
transaction state. Nested rendering can allocate more buffers; neither the
1 MiB caps nor this table bound total memory. Existing framebuffers, camera,
audio and network allocations remain live. Fragmentation matters: a contiguous
4,096,000 B top buffer is required, and a popup must still obtain a contiguous
3,072,000 B screen while that buffer is live.

## Ownership and scheduling

No handler-owned leak was found in the tested capture, allocation, PNG/JPEG
encoding, invalid encoded-size, send or disconnected-client paths. A failed
layer capture returns HTTP 500 rather than incomplete success. Unavailable
JPEG returns 501; display-lock acquisition failure returns 503. Host tests
model rendering failure as a refused snapshot; they do not cover all internal
LVGL or driver allocations.

The **1,500 ms display-lock timeout limits acquisition only**. Layout, rendering,
alpha scanning and blending all run under the lock, with no wall-clock hold
deadline. The patched LVGL snapshot stalled-dispatch counter is not a duration
guarantee. Releasing the lock midway through LVGL rendering would be unsafe.

PNG compression, JPEG processing and all HTTP sends run after unlocking:

- The HTTP server handles screenshots synchronously on core 1, priority 5, with
  a 14 KiB stack. Slow capture/encode/send delays other HTTP requests. The 10 s
  socket send timeout is not a whole-response deadline; BMP sends many rows.
  Multiple clients can queue work despite each dialog's single-request guard.
- LVGL runs at priority 20 on core 1 but cannot use the display while capture
  holds its lock. Scan/blend visits 1,024,000 pixels; PNG also swaps channels
  and compresses each row.
- IDF 6.1 `jpeg_encoder_process()` takes the shared codec mutex with
  `portMAX_DELAY` before its configured 4 s event wait. This is not an end-to-end
  deadline. HA image decoding and optional local camera JPEG output also use
  that codec. No deadlock was demonstrated by these tests.
- Audio capture/playback use priority 5 on core 0; the assistant uses priority
  4 there, and the ambient camera worker priority 3. HA's priority is 8.
  Core separation does not isolate PSRAM bandwidth, heap pressure, DMA/codec
  contention or shared locks. Priority numbers do not prove audio continuity.

Browser close/30 s timeout aborts fetch and decode, detaches the temporary Image
and immediately revokes its pending Blob URL. Late callbacks cannot replace a
newer image or revoke its URL. Downloads retain their existing short grace
period. This cancellation does **not** stop capture/encoding already running on
the device: its resources remain until completion or a detected send failure.
Browser reclamation timing remains engine-owned.

## Regression coverage and limits

`tests/host/test_screenshot.c` and `screenshot_large_cases.h` exercise production
handlers with real zlib and simulated LVGL/JPEG/HTTP:

- 1280 × 800 normal/popup/opaque-cover lifetimes, full-frame PNG pixels/CRCs,
  BMP size/row order, and JPEG input geometry.
- Every handler-side encoder allocation ordinal, including partial zlib
  initialization; either snapshot refused; engine creation/process failure;
  invalid JPEG sizes; PNG output exhaustion and deflate row/finish failures.
- Display busy then recovery; HTTP error-response and BMP header/row/end send
  failures; compressed-send failure; consecutive success/failure requests
  without resetting resource counters between requests.
- Rendering holds the lock, encoding/sending do not, and all owned buffers and
  engine handles return to zero after each request.

The ordinary `tools/test_host.sh` JPEG, no-JPEG and sanitizer targets include
these cases. Browser regressions cover body/decode cancellation, immediate URL
cleanup, late completion, retry, PNG fallback and download ownership. Injected
lock contention/disconnects are not FreeRTOS scheduling or real TCP experiments.

On 2026-11-14 Windows GCC passed both Host modes; web consistency checks passed.
The editor suite passed 580 tests in an isolated copy with six existing
catalog/font text files normalized to LF. Five original-checkout catalog tests
assume ` =\n` in generated `.inc` files and fail on existing CRLF input; catalog
content was not changed. ASan/UBSan was not run: WSL is absent and installed
ESP Clang has no x86 Host target. Linux sanitizer CI remains required.

## Required Panel10 measurements

An explicitly authorized, bounded trial must record firmware/ELF/configuration,
layout/theme and enabled camera/voice/HA workload. Compare idle and active
workloads with normal, popup and opaque-clock captures plus a slow/disconnected
client. No routine stress test is implied.

| Metric | Evidence still required |
| --- | --- |
| PSRAM/internal RAM | Stage high-water, local heap minimum, largest free block and recovery after each request; include LVGL temporary layers and encoder scratch |
| Screenshot duration | Separate lock wait/hold, top/base render, scan/blend, encode, send and total; bounded repetitions with sample count and worst observed value |
| JPEG contention | Codec wait versus encode time with actual camera output/HA cover decoding active; timeout/error behavior |
| CPU | Per-core/task runtime deltas by stage versus baseline, with sampling overhead documented |
| LVGL response | Refresh/timer progress, touch-to-visible latency, watchdog and task-stack low-water, particularly HTTP |
| Audio continuity | Capture/playback cadence, dropped frames, underruns and audible gaps during a conversation; wake recovery afterward |
| Camera and HA | Frame cadence/publish latency and HA receive/service latency/reconnects; distinguish HTTP-server delays from HA's separate task |

The 2026-11-11 `b48e186a` device trial confirmed formats, Live view and
owner-observed touch/voice behavior. Its sequential checks and post-request heap
samples did not measure simultaneous screenshot/voice peaks or prove latency
bounds. This review used no device access, configuration changes or flashing.
