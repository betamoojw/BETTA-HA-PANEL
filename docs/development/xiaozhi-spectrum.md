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
