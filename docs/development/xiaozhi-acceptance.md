<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi acceptance baseline

This is a **local, bounded acceptance record** for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1 on COM24, dated 2026-11-06. It identifies the
existing tested image; later documentation commits do not change that image.
No firmware rebuild, flash, reset or remote publication is part of this archive.

## Image identity

| Field | Accepted value |
| --- | --- |
| Firmware source | `7ec2109d` (clean source) |
| Firmware version / profile | `v0.9.1` / `panel10` |
| Runtime build stamp | `Nov 6 2026 11:04:57` |
| Device | ESP32-P4 v3.2, 32 MiB flash, 1280 × 800 |
| Toolchain / network adapter | ESP-IDF 6.1 / ESP-Hosted C6 3.0.10 |
| App size / slot capacity | 6,632,640 / 6,815,744 bytes |
| Remaining app space | 183,104 bytes; no partition change |

Source commit:

```text
7ec2109d8222cd57fe7e96c714af962174661463
```

Application SHA256:

```text
d2d6789ddef4e0621d6ca4b691cb3e7438dd40982a659bb88c83b9a86a2078ca
```

Matching ELF SHA256:

```text
37f813989e1c59fd51ed0b1b90386f92895def96e98d687e1881f631743d2977
```

The frozen app embeds this ELF digest and source stamp. The ELF's runtime build
stamp matches the captured `/api/version` and boot log. Use the exact archived
ELF for diagnosis; rebuilding the same source need not reproduce its bytes.

## Acceptance and limits

The owner accepted counting from one to twenty, interrupting a second answer,
saying goodbye and waking again, without self-recognition, false interruption
or reboot; sound, physical display and touch were also accepted. The saved
speaker volume was 80%, with full-duplex and local echo cancellation enabled.

| Evidence | Recorded result |
| --- | --- |
| Bounded monitor | 239.6 s; 433 voice-state and 16 version samples |
| Connection endings | Two valid empty server CLOSE frames; no status code |
| Close diagnosis | `close_without_status`, code 0; error count stayed 0 |
| Recovery | Standby, then a new realtime voice connection after wake |
| Concurrent audio | 793 uploaded frames during playback; 116 overlapping samples |
| Audio queues / echo reference | Zero capture drops; playback reference peak 10,867 |
| Task stack low-water | Capture 6,104 bytes; playback 3,604 bytes |
| Stability in this window | No HTTP errors, uptime decrease or new crash report |
| Startup and preservation | Matching source/ELF, OTA valid, HA authenticated; saved data retained |

`barge_ins` remained 0 in this window: interruption acceptance comes from the
owner, not an independently positive counter. Reference activity and simultaneous
upload/playback demonstrate the pipeline, not a numerical echo-reduction score.
This is not an all-day, far-field, other-room, other-volume or other-board result.
Earlier intermittent display flashes remain outside this fix. Startup logs also
contained LVGL-lock, HA JPEG-header and HA energy-preference warnings; those
paths were not repaired or qualified by the voice test.

Host regressions exercised the actual close callback, sender, teardown and
worker-ending functions: 10 scenario groups and four negative controls covered
normal/empty/abnormal/malformed closes, queued sends, timeouts, late errors,
local cancellation and reconnection. The prior TLS suite passed 11 scenarios
and four negative controls. API, session, duplex and gain tests passed too.
Injected host disconnects are not physical Wi-Fi fault tests. The close-suite
result is retained in `local-validation.json`; its original console output was
not saved as a separate log. Other test logs and raw monitoring samples are retained.

## Local archive

The ignored archive directory is under `.local/baselines/`, named:

```text
xiaozhi-panel10-7ec2109-2026-11-06
```

The sibling ZIP contains the same payload. Its SHA256 sidecar verifies the ZIP;
`SHA256SUMS` inside verifies every payload file other than the checksum list itself.
`BASELINE.json` records source, image identities, provenance and the later
documentation commit. Archives and capture files stay outside Git.

| Path inside the archive | Purpose |
| --- | --- |
| `firmware/accepted/` | Tested app, exact ELF, sdkconfig, reference partition table and image manifest |
| `source/7ec2109.zip` | Tracked source snapshot of the exact firmware commit |
| `evidence/close-fix/` | Raw samples, logs, owner acceptance, preservation checks and recorded test results |
| `evidence/transport-fix/` | Earlier TLS investigation and regression evidence |
| `evidence/crash-elf/` | Exact `de6dc171` ELF matching the two retained crash reports, plus decoded evidence |
| `comparison/e95fd64/` | Prior app/ELF and metadata; known transient close-state error, not the accepted target |
| `documentation/` | Accepted usage, limits and restore instructions, plus documentation check results |

Two retained `de6dc171` crashes have SHA256 IDs beginning `915e01ce9833c1b9`
and `9d73336a83529206`; their matching ELF begins `bd29dfb0f`. They are historical
TLS-crash evidence, not crashes of `7ec2109d`. Copies and decoded records remain
separate from the accepted ELF. The archive contains device settings, layout,
transcripts and identifiers; keep it local unless the owner chooses to share it.
Historical helper scripts in the evidence directories are records of those runs,
not portable restore commands. The source ZIP is not a complete offline build kit;
dependencies and toolchain resources are not all included. Restore the frozen app.

## Restore the baseline

These are instructions for a **future authorized restore**, not actions performed
while archiving. Restoring an app restarts the panel. First capture its current
version/tasks, logs, reboot history, layout, settings and every retained crash
report by ID; preserve each matching ELF. Do not replace newer data with the
historical settings/layout snapshots merely to return to this firmware.

1. Verify the archive and accepted app SHA256 against this page and `SHA256SUMS`.
   Match `panel10`, project `betta-ha-panel-10.1`, P4 revision 3.x and the installed
   partition layout. Check compatibility of any settings/storage changes made by
   a later firmware; unchanged partitions alone do not prove backward compatibility.
2. With a working editor and compatible layout, use **Settings → Firmware** to
   upload `firmware/accepted/betta-ha-panel-10.1.bin` as an app image. Keep power
   stable and allow validation to complete. Follow [OTA validation](release.md#rollback).
   Do not assume the bootloader's previous slot contains this baseline.
3. If USB recovery is needed, identify the board and actual port again before
   opening serial. Follow the [32 MiB preservation procedure](build.md#32-mib-p4-preservation-checks):
   use the supported P4 stub, verify the installed table and determine the active
   app slot from valid OTA metadata. The accepted trial used `ota_1` at `0x6A0000`,
   but a later OTA can change that. Write and verify only the chosen app slot;
   compare protected-region checksums before rebooting. Do not copy the historical
   offset blindly or write the reference partition table, NVS or factory image.
4. After boot, verify source `7ec2109d`, build stamp and ELF prefix `37f813989`,
   OTA state `valid`, network/HA connection, saved layout/settings and retained
   crash IDs. A responding HTTP server alone does not establish successful restore.
5. Repeat the [short voice check](../xiaozhi-ai.md#check-full-duplex-conversation)
   and collect the final close diagnosis. Keep the pre-restore and post-restore
   evidence in a new directory; do not overwrite this accepted record.

USB overwriting the active slot does not create a new bootloader rollback target.
The preserved `e95fd643` app fixes the TLS crash but still has the goodbye-state
misreport; `de6dc171` has recorded TLS crashes. Neither is a recommended substitute
for this baseline. The restore sequence is documented, not rehearsed by another
flash during this documentation task.

[User guide](../xiaozhi-ai.md) · [Connection internals](components.md#xiaozhi-connection-endings)
· [Crash evidence](../coredump.md)
