<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# AI Assistant acceptance baseline

This is the local acceptance record for **f30122b8** on the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1, COM24, dated 2026-11-06. It freezes the already
tested application and matching ELF. Archiving adds no firmware build, flash,
restart, stress test or remote publication. The earlier
[7ec2109 baseline](xiaozhi-acceptance.md) remains intact.

## Image identity

| Field | Accepted value |
| --- | --- |
| Firmware source | `f30122b8` (clean source at build) |
| Version / profile / project | `v0.9.1` / `panel10` / `betta-ha-panel-10.1` |
| Runtime build stamp | `Nov 6 2026 18:09:02` |
| Device | ESP32-P4 v3.2, 32 MiB flash, 1280 × 800 |
| Toolchain / adapter | ESP-IDF 6.1 / ESP-Hosted C6 3.0.10 |
| App size / slot capacity | 6,653,824 / 6,815,744 bytes |
| Remaining app space | 161,920 bytes; partition layout unchanged |

Source commit:

```text
f30122b8e1d322e1bf1f5f9023fd2d1c40124585
```

Application SHA256:

```text
37738aacd49296dcc055c0de853f34d2f7017282d0114c1db3b8c8386677311d
```

Matching ELF SHA256:

```text
3cbd56ca641c5a6a94e592bc6788b02b80e02912d08d2dd3c6d807da34e0b51f
```

The image embeds the source stamp and ELF digest. Build-time strings can be
cached; use the source stamp and both hashes together, not the timestamp alone.
Use this exact ELF for diagnosis. A documentation commit or a later rebuild of
the same source does not replace the accepted image.

## Acceptance evidence

During the original acceptance trial, the owner confirmed the following on the
physical panel:

- A question about the meaning of "bye bye" continued the conversation; an
  explicit goodbye closed the popup and returned to standby.
- Reawakening worked after goodbye. Counting, spoken interruption with
  "Stop, what is two plus two?", sound and echo suppression were normal.
- The last-five-second hint appeared; new speech cancelled it. Another
  uninterrupted 30-second silence interval closed the conversation.
- Popup opening/closing, display and physical touch were normal.
- The owner selected Simplified Chinese and asked to retain it.

| Recorded device evidence | Result |
| --- | --- |
| Bounded windows | 65 s connection/quiet check and 150 s owner voice trial |
| Samples | 194 assistant snapshots and 21 version/task snapshots |
| Session endings | `farewell` and `silence`, followed by standby and new sessions |
| Concurrent audio | Up to 679 uploaded frames during playback |
| Task stack low-water | Assistant 6,728; capture 6,088; playback 3,340 bytes |
| Stability | No observed uptime reversal, HTTP error, connection error or capture drop |
| Startup | Source/ELF matched, OTA state `valid`, HA authenticated, cloud speech connected |
| Preservation | Metadata/NVS, coredump, full LittleFS and inactive app matched before boot |
| Saved state at acceptance | Layout and assistant preferences retained; owner-selected `zh-cn` retained |

`barge_ins` remained 0 during the recorded trial. Interruption and acoustic
acceptance come from the owner; simultaneous upload/playback is pipeline evidence,
not a measured echo-reduction score. The device had no retained crash reports
before or after this update. Historical crash records remain in the preserved
7ec2109 archive; an empty current list does not erase that evidence.

The initial post-update check observed an unsynchronized network clock. The owner
subsequently confirmed recovery. The separate HTTP follow-up reported
`clock_valid=true` at device-local `2026-11-06T18:51:51`, with source `f30122b8`
and OTA state `valid`. Saved timezone `Asia/Shanghai` and NTP server
`0.wled.pool.ntp.org` were unchanged. Keep both observations instead of rewriting
the original test result. The 30-second policy uses monotonic time and is
independent of wall-clock synchronization.

That follow-up reported interface language `en`. The owner confirmed switching
back to English and asked to retain the current choice. Archiving did not change
device settings; the original `zh-cn` trial remains a historical observation.

Local regressions passed: eight production dialogue scenarios and four negative
controls, 510 web tests, native lifecycle tests and strict documentation checks.
Previously green audio, API, connection, transport and layout checks were reused
where their inputs were unchanged. The preserved reports identify those sources
and distinguish captured logs from summaries; these were not new device stress tests.

## Use and limits

Follow [AI Assistant](../xiaozhi-ai.md#conversation) for wake/manual controls,
farewell examples and the short conversation check. Actual wake phrases, pairing,
API/storage identifiers and service attribution retain their original identity.
Only confirmed user STT enters farewell classification. Quoted examples, negations,
questions about meaning and assistant TTS are not farewell commands.

Only effective user speech or successfully played, non-silent, unmuted audio
renews the fixed inactivity deadline. Background noise, silent PCM, queued audio,
text and network traffic do not renew it. Playback includes a conservative
120 ms I2S/DMA tail allowance; this is not millisecond acoustic calibration.
Manual controls require holding Talk to capture new speech.

Real connection faults retain diagnostics and recovery guidance, including a
failure before the first UI refresh. Explicitly closing that guidance restores
standby without clearing the last fault. Use [connection recovery](../xiaozhi-ai.md#connection-recovery)
before considering a restore; resetting pairing is not routine recovery.

This acceptance covers one device and the recorded short trials. It does not
establish endurance, far-field performance, other rooms/volumes/boards, a physical
Wi-Fi fault matrix or an acoustic AEC score. Earlier intermittent display-flash
reports remain outside this scope. No rollback was rehearsed during archiving.

## Local archive

The ignored directory under `.local/baselines/` is:

```text
ai-assistant-panel10-f30122b-2026-11-06
```

Its sibling ZIP has a SHA256 sidecar. `SHA256SUMS` inside checks payload files;
`BASELINE.json` identifies the firmware source, hashes, provenance and later
documentation commit. These raw assets remain outside Git.

| Archive path | Content |
| --- | --- |
| `firmware/accepted/` | Existing app, exact ELF, sdkconfig, reference table and manifest |
| `rollback/7ec2109/` | Unchanged copies of the earlier app/ELF, sdkconfig, table and manifest |
| `source/f30122b.zip` | Tracked source snapshot of the firmware commit |
| `evidence/implementation/` | Local regression, browser/SDL and source-hash records |
| `evidence/device/` | Original flash, preservation, startup, monitoring and owner acceptance |
| `evidence/clock-recovery/` | New HTTP identity/clock/configuration check and owner confirmation |
| `documentation/` | Baseline and operating references with documentation validation |

Diagnostics can contain device identifiers, settings, layout and transcripts.
Keep them local. Preserve the complete 7ec2109 archive separately; do not merge
its firmware or old crash ELF with this image. Historical helper scripts are
evidence, not portable restore commands. The source ZIP does not include the
entire offline dependency/toolchain environment.

## Restore the baseline

These steps describe a future, separately authorized restore. Do not perform
them merely to inspect or verify this archive.

1. Save the current version/tasks, logs, reboot history, layout/settings and every
   retained crash report by SHA256 ID. Keep the matching crash ELFs. Never restore
   old settings/layout snapshots just to return to an earlier application.
2. Check archive checksums and the app/ELF hashes above. Verify the actual board,
   silicon revision, installed partition layout and compatibility with any data
   changes made by newer firmware. Use the frozen app without rebuilding it.
3. With a working editor, upload `firmware/accepted/betta-ha-panel-10.1.bin` in
   **Settings → Firmware**. Follow [OTA validation](release.md#rollback). Do not
   assume the bootloader's previous slot contains this baseline.
4. If USB recovery is required, identify the device/port again, save HTTP evidence
   first when available, and follow the [32 MiB preservation procedure](build.md#32-mib-p4-preservation-checks).
   Determine the active slot from current valid OTA metadata. This trial used
   `ota_1` at `0x6A0000`; it is a historical observation, not a future write target.
   Write only the selected app, verify it and compare protected regions before
   booting. Do not write the reference table, NVS, data partitions or a factory image.
5. Check source `f30122b8`, ELF prefix `3cbd56ca6`, OTA state `valid`, connectivity,
   current settings/layout, clock and retained crash IDs. Repeat the short voice
   and UI checks above. Save new evidence separately from this accepted record.

USB overwriting an active app does not create a new bootloader rollback target.
The preserved [7ec2109 procedure](xiaozhi-acceptance.md#restore-the-baseline) remains
available if that older baseline is deliberately chosen; its copied image and
ELF are under `rollback/7ec2109/`. It predates the current
audio-only exit policy, farewell classifier, conversation bubbles, live spectrum
and unified product name; do not treat those newer behaviors as its acceptance.
