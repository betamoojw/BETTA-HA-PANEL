<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# AI Assistant spectrum baseline

The current local baseline is **dc55fb12**, accepted on the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1, COM24, on **2026-11-07**. The owner accepted
speech, interruption, echo suppression, reawakening, real spectrum motion,
display and touch. This records bounded functional acceptance; the performance
observations and unmeasured cases below remain part of the baseline.

Archiving reuses the tested app, matching ELF and original evidence. It adds no
device access, firmware build, flash, restart, settings change or stress test.
The [f30122b record](assistant-acceptance.md), [7ec2109 record](xiaozhi-acceptance.md)
and [8a360e2 trial](assistant-transport-validation.md#com24-8a360e2) remain intact.

## Image identity

| Field | Accepted value |
| --- | --- |
| Firmware commit | `dc55fb126e0f82d234245953334beb777143a348` |
| Version / variant / project | `v0.9.1` / `panel10` / `betta-ha-panel-10.1` |
| Runtime source / build | `dc55fb12` / `Nov 7 2026 07:58:13` |
| Device | ESP32-P4 v3.2, 32 MiB flash, 1280 × 800 |
| Build profile | ESP-IDF 6.1; CPU 400 MHz; PSRAM 200 MHz |
| App / slot / remaining bytes | 6,659,216 / 6,815,744 / 156,528 (2.297%) |

Application SHA-256:

```text
69dc5f143e6c2f150999353ca08a6a6aef0a045664a814cf76469706dea0d273
```

Matching ELF SHA-256:

```text
e945e88b4092813576ae9bc4e36821ee84e6b4bb8df0506bc090884ab0693a01
```

The image's embedded ELF digest matches the archived ELF. The source ZIP matches
a fresh Git archive of the firmware commit. Its build-time manifest still says
"unflashed candidate"; that historical record is preserved, with the later device
trial linked by the new `BASELINE.json`. A documentation commit does not change
the accepted firmware identity. Diagnose crashes with the matching ELF, not a
later rebuild that happens to share a version string.

## Device and owner evidence

The authorized trial wrote only the verified active `ota_1` application and
verified its readback. Metadata/NVS, the coredump partition, all LittleFS bytes
and the inactive app matched before boot. That slot's `0x6A0000` address is a
historical observation, not a reusable restore target.

Running source/ELF, OTA state `valid`, HA connection and synchronized time were
verified. English, pairing/wake/volume/duplex preferences, saved settings,
layout, theme and camera/KNX configuration matched after the trial, excluding
live RSSI/time readings. Boot sequence changed once, from 184 to 185, for the
update; no later restart or retained crash appeared. An empty current crash
list does not remove the historical crash evidence in earlier archives.

The owner tested a complete count-to-twenty reply, spoken interruption with a
new arithmetic question, goodbye and reawakening. They reported normal sound,
interruption and echo suppression, without self-recognition, false interruption
or restart. They also accepted the speech/playback spectrum, its quiet line,
bubbles, long-reply scrolling, popup lifecycle, physical display and touch.
The original prompts and responses are in `evidence/owner-confirmation.json`;
the trial's existing summary remains in `evidence/device/owner-acceptance.json`.

| Recorded observation | Result |
| --- | --- |
| Before / after idle windows | 60 s each, 60 samples each |
| Voice and quiet window | 300 s, 297 successful sequential samples |
| Audio activity | 110 listening or speaking/listening samples |
| Concurrent audio | 1,901 uploads, 958 received frames; 980 uploads during playback |
| Spectrum | Both 24-band sources varied; all 185 inactive samples were zero |
| End of observation | 108 s of sampled zero spectrum, then standby at final capture |
| Fault counters | No observed capture drop, connection error, HTTP failure or new crash |
| Final HTTP capture | Uptime 584 s, ready, wake detection running, clock valid |

The 584-second uptime is not a continuous 584-second measurement. `barge_ins`
remained zero: it counts recognized STT only while local TTS/playback is still
active. Server stop/replacement may precede that event; exact ordering was not
traced. Owner confirmation supplies interruption and acoustic acceptance.
AEC/capture diagnostics reset at each duplex start; their final values are not
totals across both sessions. Sampled counters cannot prove absence of DMA underruns.

## Performance observations

One observer read version/tasks and assistant status sequentially at about 1 Hz.
No browser or screenshot polling was added. Other household activity was not
controlled. CPU summaries omit the first five seconds of each observation.

| Metric | 8a360e2 idle | dc55fb1 idle | dc55fb1 voice trial |
| --- | ---: | ---: | ---: |
| Core 0 median / p95 | 13 / 14.3% | 13 / 15.9% | active 55 / 67% |
| Core 1 median / p95 | 5 / 30.3% | 5 / 29.3% | active 50.5 / 79.55% |
| Sampled internal free minimum | 133,027 B | 147,411 B | 83,859 B |
| Boot-lifetime internal free minimum | 65,416 B | 132,636 B | 67,280 B |
| Largest internal block minimum | 75,776 B | 102,400 B | 30,720 B |
| Sampled PSRAM free minimum | 8,447,116 B | 8,503,988 B | 5,186,748 B |

Active voice CPU maxima were 70% / 84%; the p95 values were below the proposed
85% initial gate. Idle p95 differences were +1.6 / -1.0 percentage points.
There is no matched preflash voice workload from which to infer an active CPU
improvement. The old image had already exercised voice, whereas candidate idle
preceded its first dialogue; memory/stack differences are not isolated savings.

| Task | Minimum free stack |
| --- | ---: |
| Capture / playback | 6,308 / 3,336 B |
| Assistant session | 6,696 B |
| UI runtime / LVGL | 10,892 / 13,176 B |
| HTTP | 10,440 B |

These exceed the initial 1 KiB audio/UI/session/HTTP floor. Report absolute free
bytes: some API stack-capacity fields are unreliable. `ha_client` still had only
712 B free, equal to the preflash observation; retain this separate low-margin
observation. No overflow was observed. RAM figures describe the P4; the C6 memory
monitor was unsupported.

## Open observations and measurement limits

- The final bounded log contains **148 LVGL lock-failure messages**, including
  speech and touch intervals. This warning category also existed before the
  update; differing log windows and workloads do not establish comparable rates
  or a cause. Keep lock contention as an open performance observation.
- Version/task HTTP RTT was p95 **206.416 ms**, maximum **2,558.366 ms**.
  Assistant status RTT was p95 **58.870 ms**, maximum **1,886.371 ms**.
  Requests succeeded, but tail latency remains unqualified. These figures are
  neither browser response-to-paint nor physical display latency.
- JPEG cover/HA energy warnings and I2S disable messages at audio transitions
  remain in the original logs. Functional acceptance does not imply an empty
  warning log. Earlier intermittent display-flash reports are outside this trial.
- No DSP-only timing, analyzer allocation plateau, three-cycle memory drift,
  controlled browser coexistence/slow-network trial, optical presentation delay
  or measured acoustic gap/AEC score was captured. A 1 Hz observer cannot validate
  subsecond spectrum settling. Silence data plus owner observation establish the
  recorded quiet-line behavior, not a latency bound.
- No endurance, other-room/volume/board or physical fault-injection qualification
  is claimed. The full [performance procedure](xiaozhi-spectrum.md#panel10-performance-acceptance)
  remains a separately scoped follow-up. Host/SDL regressions are local evidence,
  not additional real-device measurements. The f30122b silence-deadline and
  countdown-renewal trial remains historical, not a repeated dc55fb1 measurement.

## Local archive

The ignored directory `.local/baselines/ai-assistant-panel10-dc55fb1-2026-11-07/`
and sibling ZIP retain the baseline. `SHA256SUMS` checks its payload; the ZIP has
a SHA-256 sidecar. `BASELINE.json` records firmware identity, evidence provenance,
limits and the later documentation commit.

| Archive path | Content |
| --- | --- |
| `firmware/accepted/` | Original dc55fb1 app, ELF, source ZIP, config, table and build provenance |
| `evidence/device/` | Original flash/readback, diagnostics, metrics and owner acceptance |
| `evidence/local-review/` | Previously recorded host/SDL/browser regression reports |
| `rollback/8a360e2/` | Immediate earlier accepted app, exact ELF, config, table and image manifest |
| `documentation/` | Current references, integrity checks and documentation validation |

Raw settings, identifiers and transcripts remain local. Historical helper scripts
are evidence, not portable recovery commands. Preserve the separate 7ec2109 and
f30122b archives and all 8a360e2 evidence. The source ZIP is not an offline
toolchain/dependency bundle. No fresh device check is implied by archiving.

## Restore or roll back

These are future, separately authorized operations; no restore was rehearsed
during archiving. To restore this baseline, choose the frozen app under
`firmware/accepted/`. To undo this spectrum optimization, the immediate earlier
accepted target is `rollback/8a360e2/`:

```text
Source: 8a360e2aef37d50a5308236c82672a1e3e4724cc
App: 9d32a3388c92650f71fa17156828aba9f344e4eb3ad8f588ba0d4a3d1a61370f
ELF: 1545c0cf9682f2e7f83ee9d0691d2555e75c4d756b91a4f749319d5a8d7cdec8
```

1. Save current HTTP version/tasks, log, reboot history, layout/settings and each
   retained coredump by SHA-256 ID, with its matching ELF. Keep current settings
   and data; do not restore old snapshots merely to change the application.
2. Verify the archive, selected app/ELF hashes and source identity. Identify the
   current board, revision, port and installed partition table. Confirm that
   newer application data remains compatible with the chosen older firmware.
3. If the editor works, upload only the selected app through **Settings →
   Firmware** and follow [OTA validation](release.md#rollback). Do not assume
   the bootloader's previous slot contains the desired accepted image.
4. For USB recovery, first preserve HTTP evidence when available. Use the
   [32 MiB P4 preservation procedure](build.md#32-mib-p4-preservation-checks)
   and supported stub; derive the active slot from current valid OTA metadata.
   Verify the selected app and protected regions before booting. Never flash the
   reference table, bootloader, NVS, coredump, filesystem or a factory bundle.
5. Confirm the chosen running source and ELF digest, valid OTA, connection,
   synchronized time, current English/settings/layout and retained crash IDs.
   Repeat complete reply, interruption, goodbye/rewake, spectrum/quiet line and
   physical display/touch checks. Record the new trial separately.

An active-slot USB overwrite creates no new automatic rollback target. Choosing
[f30122b](assistant-acceptance.md#restore-the-baseline) or
[7ec2109](xiaozhi-acceptance.md#restore-the-baseline) instead is deliberate loss
of later fixes/features: f30122b predates the 8a360e2 transport-backpressure repair;
7ec2109 also predates bubbles, live spectrum and the current exit/name policy.
Keep their original scope and matching ELFs rather than treating them as equivalent.
