<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Panel10 font storage validation

On November 28, 2026, a paired Windows build of Panel10 reduced the application
by **57,472 bytes** and increased the free space in each OTA slot from **4,224 to
61,696 bytes**. The baseline was `dev` at `2a8989eb` with its existing packed CJK
font. The candidate added lossless storage for the four other UI fonts.
The [earlier COM24 observations](panel10-cjk-font-validation.md) apply to the CJK-only firmware.

## Font payloads

| Face | Mapped codepoints | Source TTF (bytes) | zlib level 9 | Zopfli | Raw bytes saved |
| --- | ---: | ---: | ---: | ---: | ---: |
| Noto Emoji | 196 | 87,036 | 56,259 | 55,271 | 31,765 |
| Material Design Icons | 178 | 26,916 | 14,848 | 14,345 | 12,571 |
| Poppins Medium | 230 | 17,448 | 10,823 | 10,657 | 6,791 |
| Poppins Bold | 230 | 17,128 | 10,893 | 10,730 | 6,398 |
| **Four additional faces** | | **148,528** | **92,823** | **91,003** | **57,525** |
| Existing Noto Sans SC | 979 | 286,900 | 192,227 | 188,752 | 98,148 |

Both encoders used the same committed TTFs. Zopfli 1.0.3 with
`--zlib --i15` saved a further 1,820 bytes over level 9 on the four faces.
Every stream inflated to the exact TTF and its generated raw C array. The
normal configure step verified the streams using Python's standard library.

## Paired firmware builds

Both builds used the repository's Windows Panel10 wrapper, ESP-IDF 6.1.1, the
same isolated source/build path, C6 image, dependency lock and effective
`sdkconfig`. A documentation-only marker gave both builds the same
`2a8989eb-dirty` source label. `SOURCE_DATE_EPOCH=1795824000` fixed their
build stamps. The configuration JSON, partition binary and vendor zlib archive
were byte-identical. The generated configuration text and header differed only
in the order of entries. Each application descriptor matched its archived ELF
SHA-256. Both offline image checksums and validation hashes passed.

| Output, bytes | CJK-only baseline | Five packed fonts | Reduction |
| --- | ---: | ---: | ---: |
| Application BIN | 6,811,520 | 6,754,048 | 57,472 |
| ELF with symbols | 48,837,740 | 48,779,020 | 58,720 |
| Linker MAP file | 15,428,053 | 15,420,667 | 7,386 |
| Linked `.flash.rodata` | 3,936,420 | 3,878,948 | 57,472 |
| Linked `.flash.text` | 2,758,752 | 2,758,752 | 0 |

The partition binary describes two 6,815,744-byte slots at `0x20000` and
`0x6a0000`. Each candidate BIN contains exactly one copy of each compressed
stream. ELF symbols and binary content exclude the five raw arrays. The final
BIN reduction is 53 bytes smaller than the four streams' payload reduction.

| SHA-256 | Baseline | Candidate |
| --- | --- | --- |
| BIN | `7a5d6e18121ea2bbf2e863d4d8dec6ff0b44af487439be0b8e5cb3f22c31f460` | `b84c6d58279bfe93d5f43251b1d45c68a37e65e61cf45c661ecaccf14edefb36` |
| ELF | `f02dacf7ea5c01299e999c1d0ef63c24856c9ca9f33c3672398997b849b3c29a` | `6b24e57572dc7349ac3ed60faa2844ed408f68a0e65c49e0dca0f8bd63ec9d98` |
| MAP | `c94f00df4319ca87565aa0ef15b5f7a4eaff8af67543e184d8d60ecc509c9356` | `570f66b8b22a641becea1b37391b1d5aa893962dc812769cc9a9b0fc3d5bc4e4` |

## Memory and first use

One PSRAM allocation retains the decoded bytes of each requested face for the
rest of the boot. All five together retain 435,428 bytes, including the
baseline's 286,900-byte CJK allocation. The four new faces add 148,528 bytes of
decoded buffers. With `CONFIG_SPIRAM_RODATA`, replacing 148,528 raw bytes with
91,003 packed bytes also reduces the read-only payload in PSRAM. The net font
payload estimate with all four loaded is **+91,003 bytes**. Actual free heap
also depends on MMU page rounding, allocator metadata and live LVGL objects.
The page-rounded read-only segment in the two ELFs spans 61 and 60 pages of
64 KiB respectively. The cache pointer array grows from 4 to 20 bytes.

The Windows x64 production-decoder harness used the locked zlib source.
During each successful decode, peak requested heap was the output size plus
7,152 temporary bytes, released at completion. The matching P4 ELF reports
7,120 bytes for `struct inflate_state`. Device allocator overhead and
fragmentation require hardware measurements. A separate optimized host run
measured 201 cold calls per face with a high-resolution timer.

| Face | Median cold decode (µs) | 95th percentile (µs) |
| --- | ---: | ---: |
| Poppins Medium | 85.9 | 89.8 |
| Noto Emoji | 385.7 | 409.7 |
| Noto Sans SC | 1,372.2 | 1,792.6 |
| Material Design Icons | 132.0 | 141.5 |
| Poppins Bold | 87.0 | 99.7 |

The first regular text size also requests emoji and CJK. Bold and MDI load
when their roles are first used. A further 100,000 accesses per face needed
zero decoder allocations.

To measure initialization of the complete first text font, 51 pairs of fresh
Windows processes alternated the two firmware storage paths. After
`lv_init()`, the timer ran from entry to return of `ui_font_text_px(20)`,
including creation of all three tiny_ttf instances. The CJK-only baseline
measured 1.358 ms median (1.752 ms p95). The candidate measured 1.835 ms
median (2.330 ms p95), an increase of **0.477 ms** between medians.
The next 96 requests for the same size reused the font. This measures one
host initialization call, excluding application boot and LCD output.

## Rendering and failures

- Six packer/host tests passed, including 564 C assertions. Cases covered
  stale or corrupt input, a bad checksum, truncation, trailing input, expanded
  length, allocation failure at each stage, cleanup, retry, independent faces
  and reuse across 96 size requests. The raw accessor returned the original
  pointers and lengths for all five faces.
- With the actual LVGL manager and the packed decoder, denying the first
  Poppins allocation returned the built-in font. Denying Bold kept its regular
  base, denying MDI returned the built-in fallback and denying emoji left it
  optional. Restoring allocation allowed subsequent requests to load each
  face. Clock, measurement, emphasis and emotion instances reused the buffers.
- LVGL compared all 1,813 face/codepoint pairs at 18 and 42 pixels against the
  original arrays. All 3,626 comparisons matched in advance, offsets, bounds
  and raster pixels, including every MDI and monochrome emoji codepoint.
- A fixed-clock Panel10 SDL dashboard used the baseline raw simulator fonts
  and then the candidate packed path with the same remaining objects. Nine paired
  1280×800 BMPs matched byte for byte, covering English, German, Spanish,
  French, Polish, Simplified and Traditional Chinese, mixed Chinese text and
  a bold notification. Both builds passed 486 font-budget and 437 notification
  assertions. The ordinary simulator build retained its raw font storage.

For another build, run `python tools/generate_widget_fonts.py --check`,
`python tests/tools/test_pack_cjk_font.py`, and the simulator's
`--regression fonts` and `--regression notification`. Build both firmware
trees with the same Panel10 configuration and use the partition binary and
application BIN for capacity. The Windows A/B archive contains each BIN, ELF,
MAP, configuration, hashes, rendered pairs and host logs.

Real Panel10 cold-start latency, time under the display lock, minimum free
PSRAM and largest free block alongside camera, voice and Live Display still
require on-device measurement. Physical LCD and touch behavior for the
five-font binary were outside this local run.
