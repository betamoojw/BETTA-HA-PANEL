<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# AI Assistant conversation display

For operation, see [AI Assistant](../xiaozhi-ai.md#conversation). The panel and
editor present the current recognized speech and answer as separate bubbles.
The panel retains its manual-versus-wake origin and emotion preference.
A bounded scroll area keeps long text above the spectrum
and manual Talk button. Display updates do not acknowledge speech or interrupt it.

## Audio observations

`xiaozhi_spectrum_dsp.c` analyzes 16 kHz mono PCM using a 256-sample Hann-windowed
FFT, with one window every 512 samples (32 ms). Twenty-four contiguous frequency
bands span 62.5 Hz through 8 kHz. Each band takes its largest FFT magnitude;
the display range is -60 to 0 dBFS, encoded as 0–255. DC is removed before
windowing. These are frequency magnitudes, not randomized bars or copies of an
audio peak counter. They are not calibrated sound-pressure measurements.
Exactly constant windows, including digital silence, bypass the FFT after DC
classification. Quiet nonconstant PCM retains the same analysis and cadence.

The duplex capture observation follows AEC. The speaker observation follows a
successful codec write; it does not visualize packets merely waiting in a queue.
Manual and alternating conversations use their existing mono capture/output.
Wake-only audio is excluded. Analysis never modifies PCM and has no influence
on VAD, Opus, transport, AEC calibration or confirmed-speech interruption.

Each source owns its analyzer state in PSRAM. Failure to allocate visualization
state leaves conversation audio available. There is no additional audio task.
Only a small band snapshot is shared under a short critical section; FFT work
runs outside it. Generation checks discard output from an in-flight write after
a flush. Source clears and a 250 ms freshness limit prevent old audio from
appearing active after a stop or disconnect.

## Rendering and API

`GET /api/xiaozhi` includes `spectrum.microphone` and `spectrum.speaker`, each
containing 24 integers, plus `mic_active` and `speaker_active` freshness flags.
Fresh silence can have an active source with all-zero bands. The values expose
only the latest spectrum, without recording or returning PCM.

Both views prefer fresh speaker data while it is available, then microphone
data. This choice does not pause duplex microphone upload. The native view draws
the segmented bars in one LVGL object and reads a small snapshot separately from
the conversation text. Hidden views stop redrawing. The browser polls more often
only while a conversation is active and the settings section is visible; it
discards stale samples and stops on hidden, disabled or failed status reads.
Neither view adds fabricated activity during silence or waiting.
The native timer runs at 50 ms while open and 250 ms while closed. Status/text
is read at 250 ms; only changed text, fonts, geometry or control visibility
requests layout. Zero-height bands keep a dim central cell, including fresh
silent PCM. Unchanged browser bands do not rewrite inline styles. Leaving the
tab or section cancels the in-flight GET and invalidates its revision, so a
quick return cannot accept that old response even if parsing already finished.

Transcript and reply fields each hold up to 511 UTF-8 bytes, ending at a complete
code point. They show the current text, not an unbounded conversation history.
A changed reply can reset its scroll position; an unchanged long reply cannot.

The [session policy](assistant-session.md) owns the fixed 30-second audio
inactivity deadline and farewell closure. Both views consume its remaining time
for the last-five-second hint; the spectrum is not a speech detector or timer.
Normal ends close the conversation view and clear its bars. Real connection
failures retain diagnostic fields and show the recovery hint. The web settings
page remains available after its conversation view closes.

## Validation boundary

Host tests exercise tones, silence, amplitude scaling, input chunk boundaries,
snapshot freshness and reset behavior. Native lifecycle tests cover bubble
bounds, long text, source selection, manual controls and object teardown. Editor
tests cover valid and stale spectrum payloads and conversation-state transitions.
These checks and a panel10 build do not establish real-device scheduling cost,
physical display appearance or acoustic acceptance of later session changes.
Retain the [7ec2109 acceptance baseline](xiaozhi-acceptance.md) and other accepted
images; a session-policy change needs its own authorized device trial.

## Scheduling and memory budget

The 256-point radix-2 transform has 1,024 butterflies, eight sine/cosine pairs
and up to 24 square roots/logarithms. At 16 kHz the hop permits 31.25 transforms/s
per source, or 62.5/s for two continuously fed sources. Chunking can bunch more
than one transform into a manual feed; this is not a periodic background task.
The constant-window shortcut does not reduce the rate of nonconstant noise.

Each analyzer uses 3,080 bytes: three 256-float arrays plus two counters.
The two retained allocations total 6,160 bytes in PSRAM, excluding allocator
metadata, snapshots and the LVGL view. They do not grow with conversation length.
There is no allocation in `feed`, and allocation failure disables only that
source's visualization. Shared critical sections copy at most 48 band bytes;
the transform runs outside them, with one producer per source and generation
checks on reset. Host stubs do not measure multicore lock contention.

On panel10, `xz_capture` and `xz_playback` run at priority 5 on core 0.
Capture analyzes AEC output before framing/VAD/upload. Playback analyzes after
each successful write of at most 160 samples (10 ms). Both therefore pay the
FFT cost inline. The session worker handles manual/alternating audio. UI work
runs separately; it never calls the FFT or waits for it under the display lock.
The existing `aec_processing_us` covers AEC only, **not** spectrum processing.

The native view has one drawing object, not 216 child objects: 24 central cells
through 216 cells at full scale. It invalidates on changed quantized heights,
uses immediate attack and a 50 ms half-distance release, and retains the 24-band
layout. Constant text must not trigger a second full-card layout at 4 Hz.
Otto GIF decoding is a separate cost; compare classic and Otto independently.

The browser uses one GET at a time: up to 5/s during a visible active dialogue,
1/s for visible idle settings, and no assistant GETs while hidden. GETs time out
after 8 s. Each response includes the complete status/text and 48 band numbers;
this creates JSON allocations and HTTP work on core 1, not just a tiny spectrum
transfer. cJSON uses the application's PSRAM-first hook with internal fallback
when installed. Multiple open browsers multiply this cost. Do not increase the
poll rate to match native 20 Hz without measuring HTTP and audio coexistence.

Device snapshots expire at 250 ms. Browser samples expire at 350 ms measured
from request start, checked every 100 ms, with a 100 ms CSS release. These are
separate budgets, not a 350 ms microphone-to-screen guarantee. A slow network
deliberately yields a quiet line. Hidden-tab timer throttling is not an animation
clock; visibility/page lifecycle events cancel old requests immediately.

## Visual review

The centered accent-colour cells match the existing semantic palette, bubble
surfaces, typography and four appearance styles. The footer stays below the
independent text scroller; 24 bands occupy up to 528 px inside the 960 px native
card. Keep this density and the restrained dim silence line. More bars, glow or
larger motion would consume reading space without adding useful speech feedback.
Check Light as well as Dark; a low-contrast idle line is secondary information,
not the only indication of connection state.

Native height has four pairs of cells above its center; the browser has five.
Native 20 Hz and browser 5 Hz plus CSS interpolation are not identical motion.
The views also select fresh speaker data before microphone data, even when the
speaker frame is silent. They are a combined conversation indicator, not two
independent meters or a calibrated analyzer. A source crossfade or matching cell
quantization is a separate visual change; retain measured silence and reduced
motion, and validate it against fast user/assistant switches before adopting it.

## Reproduce the host checks

Run the spectrum and UTF-8 cases in `tools/test_host.sh`, the production dialogue
policy tests and `node --test tests/webui/*.test.mjs`. The SDL
`xiaozhi-view-lifecycle` CTest covers ten minutes of **virtual** steady dialogue,
scroll preservation, source switching, dim silence, five geometries and teardown.
Render short/long text in all four styles, Dark/Light and Standard/Largest text.
Use the fixed simulator clock; save source hashes and captures under `.local/`.

Optional host microbenchmark, from the repository root:

```sh
cc -std=c11 -O2 -I main/xiaozhi tests/host/benchmark_xiaozhi_spectrum.c \
  main/xiaozhi/xiaozhi_spectrum_dsp.c -lm -o /tmp/betta-spectrum-bench
/tmp/betta-spectrum-bench
```

It reports five batches of 20,000 windows for silence, DC and a tone using the
host C `clock()`. Run old/new binaries alternately on an otherwise quiet host.
Keep compiler flags, hashes and every batch, not only the fastest value.
Neither these timings nor SDL virtual time predict P4 CPU, PSRAM bandwidth,
DMA/display latency, physical smoothness or audio continuity.

## Panel10 performance acceptance

The [dc55fb12 baseline](assistant-spectrum-baseline.md) records a completed
bounded COM24 functional trial and CPU/memory/stack observations. It does not
complete every measurement below; DSP, presentation latency and matched active
A/B costs remain unqualified. This fuller procedure does not authorize replacing
an accepted image. First collect the current HTTP identity/tasks, logs, reboot
history and crash IDs; retain exact app/ELF
and all earlier baselines. Confirm board revision, current address, source/ELF
stamp, CPU/PSRAM clocks, rotation, theme, font size, Otto/classic, volume, Wi-Fi
quality, dashboard and open browser count. Keep settings unchanged.

Use the accepted image for an initial read-only observation. A candidate A/B
comparison requires a spare matching panel or a separately approved app-only
update. Match the inputs and observer traffic; do not compare a busy household
dashboard with an empty simulator layout. A successful build does not authorize
deployment or establish hardware acceptance. Do not use the generic stress runner.

Observe closed/idle for 60 s, microphone speech for 60 s, an audible long reply
for 120 s, and a 60 s dialogue with several natural interruptions and closes/
rewakes. Repeat the speech phases once with the editor visible and once hidden.
Keep a 30 s quiet recovery observation. These are ordinary bounded interactions,
not a continuous load or endurance qualification. Stop and save evidence on a
new crash, reboot, capture drop, stall or audible discontinuity.

For each phase, poll `/api/version?tasks=1` once per second in a single sequential
client. Fetch `/api/xiaozhi` on the same cadence when no editor is observing; when
the editor is active, save its existing responses instead of adding another
5 Hz client. Record monotonic request start/end, response bytes, status and errors
to a new ignored JSONL file. Never log credentials. Exclude the first CPU sample
and the initial warm-up interval; missing fields are unavailable, not zero.

| Measurement | Method and initial candidate gate |
| --- | --- |
| CPU | Record `cpu.load_percent` per core and task deltas `100 * delta(run_time) / delta(total_time)` for `xz_capture`, `xz_playback`, `xiaozhi`, `lvgl`, HTTP and idle tasks. Reject counter wrap/restart intervals. Report median/p95/max; aim for p95 below 85% per core and candidate increase no more than 3 percentage points under matched load. |
| DSP | A separately instrumented candidate timestamps each feed and counts transformed/constant windows per source. Aggregate without per-frame logging. Aim for p95 <1 ms, p99 <2 ms and max <5 ms for 10 ms playback chunks; sum both sources' CPU time separately. Present feed wall time and task CPU time distinctly. |
| Memory | Record free/minimum/largest internal heap, free PSRAM and `lvgl_mem` live/peak bytes/allocations. Confirm the 6,160-byte analyzer plateau and no allocations per feed. After warm-up, three close/reopen pairs should show no monotonic retained growth; investigate >4 KiB internal or >16 KiB PSRAM drift beyond matched baseline noise. |
| Stacks | Record each task's `stack_free_min` in bytes. Initial floor 1 KiB for audio/UI/session/HTTP tasks, with no deterioration from the accepted image under matched inputs. Missing/deleted tasks do not have zero free stack. |
| Native latency | Instrument publication, UI consumption and actual display flush completion with the same monotonic clock. Aim for publish-to-present p95 <=100 ms, p99 <=150 ms; visible silence/stop <=400 ms. A draw callback alone is not presentation. |
| Browser latency | Record request RTT and next painted bar update with browser monotonic time. Aim for RTT p95 <=200 ms and accepted-response-to-paint p95 <=150 ms. At 450 ms simulated RTT, reject movement and recover on fresh responses; foreground stale clearing has up to about 800 ms combined source/watchdog/CSS budget. |
| Audio | Require zero new `audio_capture_drops`, connection faults or restarts in normal-network phases; verify simultaneous upload/playback and owner-confirmed interruption/AEC. Instrument playback queue starvation and driver write intervals, and record actual output in a controlled trial to detect unexplained gaps >20 ms during continuous audio. HTTP frame totals alone cannot prove no DMA underrun. |

These are starting acceptance gates, not claims about the installed panel. Save
the matched baseline distribution before deciding whether a threshold needs
adjustment. AEC statistics, API RTT and screenshot fetch duration must not be
substituted for DSP time, acoustic echo reduction or optical display latency.

Use browser developer-tools latency emulation for the slow-view case; it should
not impair the device's cloud voice connection. To measure the physical response,
use a synchronized audio marker and high-frame-rate camera viewing the bars;
record frame rate and timestamp error. Do not drive unverified board GPIOs or
run `/api/screenshot.bmp` continuously during timing measurements.

If nonconstant FFT cost breaches the measured budget, compare cached twiddles
or an ESP-DSP implementation against tone/noise golden bands before adopting it.
Moving analysis to a worker requires a bounded latest-frame mailbox and explicit
generation/overflow rules; do not add a PCM queue merely on the basis of host
timings. A compact spectrum endpoint or consumer lease likewise needs measured
HTTP/PSRAM benefit before changing the current compatible status interface.
