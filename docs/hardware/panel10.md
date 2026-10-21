<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Waveshare 10.1-inch panel

<figure class="board-hero">
  <a href="../img/hardware/panel10.png"><img src="../img/hardware/panel10.png" alt="Waveshare LCD-X controller assembly and 10.1-inch 1280 × 800 display, with USB TO UART, BOOT, RESET and external-speaker output information"></a>
  <figcaption>Illustration based on Waveshare's LCD-X hardware diagram and 10.1-inch dimensions.</figcaption>
</figure>

This guide shows the Waveshare **ESP32-P4-WIFI6-Touch-LCD-10.1** from the
LCD-X series. The `panel10` firmware profile provides 1280 × 800 geometry and
uses the Waveshare LCD-X BSP with JD9365 display and GT9271 touch support.
The profile targets ESP32-P4 Rev3.x; verify the chip revision before flashing.

## Specifications

| Item | Device / firmware profile |
| --- | --- |
| Display | 10.1-inch, 1280 × 800 |
| Dashboard area | 1280 × 680, after the top and bottom bars |
| Host / Wi-Fi | ESP32-P4 at 360 MHz / ESP32-C6 through ESP-Hosted |
| Flash / PSRAM | 32 MB flash / Hex PSRAM at 200 MHz |
| Display / touch controller | JD9365 / GT9271 |
| Navigation | Up to 12 pages |
| Camera / sound | No configured ambient camera; ES8311 and external-speaker connector, no enabled BETTA audio driver |

## Firmware and revisions

Use the `panel10` factory image when the release lists your device and silicon revision.
Follow [Install and set up](../getting-started.md#2-flash-the-panel) for browser installation.

Use the exact `panel10` image. Follow [installation](../getting-started.md)
for packaging and first-run setup.

## Buttons and ports

For flashing, connect a USB data cable to **USB TO UART (USB-C)** on the I/O
board shown above. It is the upper of the two USB-C ports; the lower **USB OTG**
port has a different role.

## Validation status and next checks

On 2026-10-21, the COM24 ESP32-P4 v3.2 unit (32 MB flash and 32 MB PSRAM)
booted the LCD-X adaptation based on `3a77ecd0`. USB writes passed hash checks,
the running ELF matched, and ESP-Hosted updated the C6 to 2.12.13. The panel
joined Wi-Fi at `192.168.71.120`, reported a valid app and authenticated to HA.
The owner confirmed correct setup-screen colors/orientation, a stable dashboard,
accurate touch and normal local page switching. API navigation and a 1280 x 800
screenshot passed. The complete original P4 flash was backed up locally.

The owner subsequently confirmed that Live Display returns the mirrored screen
in the web editor. Two earlier automated browser trials reported busy and logged
display-lock timeouts; their cause and repeatability have not been established.
Backlight optical measurements, remote-touch input, P4 application OTA and
long-duration stability have not been verified.

On 2026-10-21, clean commit `0a551a2` was reflashed on the same COM24 unit
using esptool 5.3.1 and the P4 flasher stub with GD25Q256 flash. In one stub
session, application write/readback passed and pre/post-write MD5 checksums
matched for metadata/NVS, the coredump partition and all 18.625 MiB of LittleFS
through the 32 MiB boundary. Firmware remained stopped until comparison passed.
After boot, the source stamp and valid OTA state matched; layout and saved
settings were preserved (time, clock-valid status and RSSI are live values).
Boot splash exit and touch initialization appeared in the log. The separate
clean-build display trial also passed owner-confirmed screen stability, local
touch alignment and Live Display following local page changes.

The earlier ROM-only storage digests are not valid full-filesystem evidence:
ROM cannot address beyond 16 MiB. Use the [stub preservation procedure](../development/build.md#32-mib-p4-preservation-checks).
This verifies bytes before boot; normal boot diagnostics may subsequently write NVS.

## Technical notes

Pin assignments, schematic checks, BSP versions and open points are in the [device technical notes](device-notes.md#panel10).

## Manufacturer references

- **Documentation:** [Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X#esp32-p4-wifi6-touch-lcd-101), including the LCD-X hardware diagram and 10.1-inch dimensions.
- **Flashing instructions:** [Waveshare LCD-X firmware flashing](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Firmware-Flashing), including download-mode recovery.

## Related guides

[Install and set up](../getting-started.md) · [Display and brightness](../display.md) · [Home Assistant setup](../home-assistant.md)
