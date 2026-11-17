<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Screenshot resources

Scope: the on-demand **Live view** toolbar action and `/api/screenshot.bmp`,
`.png` and `.jpg`. **Live Display** is the separate WebSocket mirror/remote-touch
feature. Host and simulator results do not qualify device performance.

## Why region capture

At 1280 × 800, packed RGB888 needs **3,072,000 B**, and ARGB8888 needs
**4,096,000 B**. The former whole-screen capture requested each in one allocation.
A popup could retain both, requiring 7,168,000 B of pixel payload before encoding.
Even a transparent top layer first needed the full ARGB allocation.

The Panel10 diagnostic trial on 2026-11-15 (`80b82b52-dirty`, ELF prefix
`383581c461da`) recorded normal-page and popup failures at `top.allocate`:
about 6.85 MB total PSRAM remained, but the largest free block was **2,162,688 B**.
Both full-screen allocations exceeded that block. Early failure held the display
lock for about 5.0–5.5 ms. These observations identify contiguous-allocation pressure;
they do not establish that the license warning caused fragmentation.

Capture now renders 32-row regions into separate RGB bands, reusing one ARGB
workspace for top-layer composition. It preserves full resolution, BGR888 pixels,
alpha blending, opaque full-screen views and popup order. No display framebuffer
is borrowed or changed. The regional renderer adapts the pinned LVGL snapshot
implementation, retaining its MIT attribution and stalled-task cleanup.

## Panel10 memory model

At 1280 × 800 there are 25 RGB bands of **122,880 B**, totaling 3,072,000 B.
ARGB scratch is **163,840 B**. Bands use 16-byte-aligned PSRAM; when the SDK
reserves unencrypted PSRAM, input bands use that pool for JPEG DMA compatibility.
The final RGB frame remains immutable until encoding or BMP transmission ends.

JPEG reserves a **1,048,576 B** output and one aligned 1 MiB workspace before
capture, borrowing the workspace as ARGB scratch during capture and
reusing it for each hardware-encoded band after unlocking. BMP has only a
54-byte stack header and streams rows. PNG allocates encoded output as needed
in 8 KiB PSRAM blocks after capture. These are incremental application pixel
payloads, excluding allocator overhead and other tasks:

| Path | Normal, popup or opaque full-screen top | Largest application allocation |
| --- | ---: | ---: |
| Capture / BMP | 3,235,840 B (3.086 MiB) | 163,840 B |
| PNG capture | 3,235,840 B (3.086 MiB) | 163,840 B |
| JPEG capture / encoding | 5,169,152 B (4.930 MiB) | 1,048,576 B |

PNG frees ARGB scratch before encoding. Its encoding upper bound includes RGB
bands, a 3,841 B row, a 4,096 B compressed-chunk buffer, 32,768 B of zlib work
arrays, `deflate_state` and allocated output blocks. The installed P4 ABI uses
5,836 B for `deflate_state` and 8,200 B per output block including metadata;
the Windows ABI uses 5,920 B and 8,208 B respectively. At the 1 MiB output cap,
128 blocks give a 4,168,141 B encoding bound on P4. Flat UI images use far fewer
blocks. This is a payload model, not a device measurement.
Normal JPEG uses more total payload than the
former 4,120,576 B encoding path, in exchange for removing its 3 MB contiguous
input requirement. Popup capture no longer retains two full-screen surfaces.

Add LVGL draw descriptors, temporary opacity/transform layers, image/font
caches, JPEG engine/DMA state and all existing display, camera, voice and network
allocations. A small band does not bound every internal LVGL allocation, and
sufficient total free memory does not guarantee successful allocation. The
1 MiB encoded-output limits remain: exceptionally complex images can still fail
encoding, with cleanup and an HTTP error rather than a truncated image.

For S3 at 480 × 480, capture needs 691,200 B of RGB bands plus 61,440 B ARGB
scratch: 752,640 B. PNG no longer requests a contiguous 1 MiB output buffer,
which failed in the [447fbf7 device trial](panels3-447fbf7-validation.md).
Row conversion, compression and encoded blocks are explicitly PSRAM-only;
they cannot fall back into the network's internal-RAM reserve. Highly detailed
images can still exhaust total PSRAM. See [S3 authentication and PNG memory](panels3-auth-png-memory.md)
for local fault injection and the build, and the separate
[COM18 validation](panels3-auth-png-validation.md) for native dark/light PNGs,
measured memory recovery and the remaining hardware gaps.

## Encoding and ownership

The capture path briefly locks to read geometry, allocates bands outside the
lock, then locks again and revalidates geometry. It renders every band under
that second lock, so a page change cannot mix two UI states in one image.
No object coordinates or display configuration are changed.

PNG converts each immutable band row from BGR to RGB and uses the existing zlib
settings. Multiple CRC-checked IDAT chunks carry one continuous zlib stream.
The complete bounded PNG is assembled before sending any image bytes, then
sent from its linked blocks. BMP retains bottom-up rows and padding.
Hardware JPEG still uses
RGB888, YUV444 and quality 90, encoding 32-row MCU-aligned bands. The assembler
checks baseline headers, dimensions, quantization/Huffman tables and scan
selectors; inserts DRI/restart markers; and writes a single final EOI. It rejects
incompatible, incomplete or oversized output before sending any image bytes.
It does not concatenate independent JPEG files. Codec/header changes need the
independent decoder regression and a device check before release.

Every partial band list, scratch/output allocation and encoder handle is
released on failure. Successful JPEG/PNG releases raw bands and scratch before
HTTP transmission; the final output is released even if sending fails. BMP
retains bands until sending ends or fails. Render/allocation failures return
500, unavailable JPEG returns 501 and display-lock acquisition failure returns
503. The browser's existing close/timeout cancellation releases its pending
image/Blob URL; it does not cancel capture or encoding already executing on the
panel. Device resources remain until completion or a detected send failure.

## Scheduling and diagnostics

The **1,500 ms lock timeout limits acquisition only**. Layout, regional drawing
and alpha blending remain under the display lock, with no wall-clock hold
deadline. Releasing it between bands would permit mixed-page captures; releasing
it in the middle of LVGL dispatch is unsafe. Repeated region traversal may cost
more CPU than a whole-screen render. The stalled-dispatch counter is not a time
guarantee. JPEG compression, PNG compression and HTTP sends run unlocked.

- Screenshots run synchronously in the HTTP task, core 1, priority 5, with a
  14 KiB stack. Requests on that server serialize, but slow capture/encode/send
  still delays other HTTP requests. The socket timeout is not a whole-response
  deadline. Browser request guards do not prevent other clients queuing work.
- LVGL runs at priority 20 on core 1 and waits while capture holds the lock.
  One capture still processes all 1,024,000 pixels, regardless of band size.
- IDF 6.1 JPEG processing waits on the shared codec mutex with `portMAX_DELAY`
  before its configured 4-second event wait. Each band releases that mutex;
  25 calls are not a 4-second total deadline. Camera JPEG and HA image decoding
  can contend for the codec. No deadlock or timing bound is established here.
- Audio capture/playback use priority 5 on core 0; the assistant uses priority 4,
  the ambient camera worker priority 3, and HA priority 8. Core separation does
  not isolate PSRAM bandwidth, heap pressure, DMA, codec contention or shared
  locks. Priority numbers do not prove audio continuity.

Each capture attempt logs `Screenshot ... capture=...` after unlocking:

- `display.lock` and `display.geometry` distinguish acquisition or geometry
  failure; `frame.allocate`/`top.allocate` identify RGB/ARGB allocation failure;
  `top.render`, `screen.render` and `cover.render` identify the regional draw path.
  `complete` means capture succeeded, not encoding or sending.
- `band` is the zero-based failing band, or the band count on success. `size`
  is display geometry. JPEG output/scratch reservation can fail before capture starts;
  that path returns an allocation-specific HTTP error without a capture record.
- `psram_free`/`psram_largest` sample `SPIRAM | 8BIT` before cleanup. Consecutive
  reads are neither an atomic reservation nor whole-device peaks.
- `wait_us` totals both lock acquisitions; `hold_us` totals the geometry probe
  and capture hold, including heap sampling, excluding unlocked allocation,
  logging, encoding and sending. Neither metric imposes a new deadline.

There is no periodic sampler, retained setting, partition change or HTTP stack
increase. Device timing, fragmentation and stack high-water need measurement
with a matching firmware/ELF and the actual concurrent workload.

## Regression coverage and limits

`tests/host/test_screenshot.c` exercises production handlers with real zlib and
simulated LVGL/JPEG/HTTP. Cases cover 1280 × 800 and portrait/odd dimensions,
normal/popup/opaque-top pixels, PNG CRCs and BMP padding/order; a synthetic
2,162,688 B per-allocation ceiling and 6,850,000 B total budget; every application
and zlib allocation ordinal; first/middle/last render failures; both lock
acquisitions; geometry changes; codec creation/process/header failures; output
exhaustion; failed HTTP sends; and recovery with zero owned allocations.
The 480 × 480 PNG cases also impose a 900 KiB total / 64 KiB contiguous limit,
check all decoded pixels for base/popup/opaque overlays, and inject failure at
every allocation and HTTP chunk, including the terminating send. Incompressible
content checks total-memory exhaustion and recovery separately from the output cap.

`tests/tools/test_screenshot_jpeg_join.py` compiles the production assembler,
creates real baseline JPEG bands and independently decodes the result with
Pillow. It compares decoded pixels with a whole-image reference, exercises
restart-number wrap and odd final dimensions, rejects incompatible/truncated
bands and verifies the 1 MiB limit. It runs in the existing tooling-test stage.

`tests/simulator/screenshot_regression.c` uses real LVGL to compare banded pixels
with the former full-screen composition, including the production license
warning before/at 30 minutes, rounded/translucent popups, shadows, opaque
clock overlays, stalled draw queues and restored display state. Its Panel10
geometry is an automated test, not a physical-device performance result.
The seven-language warning lifecycle test covers the 30-minute boundary,
touch targets, fullscreen priority, state changes and timer cleanup.

The Host JPEG/no-JPEG/sanitizer targets retain the lifecycle cases. Browser
cancellation tests remain unchanged. Windows native runs do not replace Linux
ASan/UBSan or real FreeRTOS/TCP scheduling measurements.

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
