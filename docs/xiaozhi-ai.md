<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi AI

Xiaozhi is an optional cloud voice assistant for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1 (LCD-X, `panel10`). It is off by default.
Other board variants report voice audio as unsupported.

## Pairing

1. Open **Settings → Xiaozhi AI** in the web editor.
2. Enable the assistant, then select **Pair / check pairing**.
3. Sign in to [xiaozhi.me](https://xiaozhi.me) yourself and enter the code shown
   by the panel. Pairing retries expire after three minutes.
4. Wait for **Ready**. If pairing expired, start it again.

Pairing sends the device identifier and firmware information to the Xiaozhi
service. Audio goes to that service only during a requested conversation.
This feature does not use a wake word or continuously listen for one.
It requires internet access and an account with the cloud service.

## Conversation

Open **Xiaozhi AI** on the panel's top bar. Hold **Talk**, speak toward the
microphone, then release to request a reply. The browser's Talk button also
uses the **panel microphone**, not the computer microphone. Capture ends after
at most 60 seconds even if the browser disappears.

The reply plays through the panel speaker and its latest text appears on screen.
Press the top-bar button again or use **Disconnect** in the editor to end the
session. Disable the assistant to stop pairing, capture and the cloud connection.
Volume is limited to 80%; start low when first testing a connected speaker.

Controls on this settings page save immediately. **Reset to defaults** disables
the assistant and restores 50% volume while retaining cloud pairing. Disabling
and re-enabling also preserves pairing and volume. Pairing credentials remain
on this physical device; they are not included in ordinary layout/settings
exports and are never returned by the editor API.

## Chinese interface

Simplified Chinese (`zh-cn`) and Traditional Chinese (`zh-tw`) have independent
built-in catalogs for the editor and native LVGL panel. Interface labels and
status messages are selected locally, without an online translation service.
The editor bundles its catalogs; the panel embeds its catalogs and CJK font.
Local custom translations may override the built-in wording.

This does not make speech recognition or answers offline. Those remain cloud
services; reply text is displayed as received, without automatic conversion
between simplified and traditional Chinese. The embedded font covers built-in
interface text, not arbitrary Chinese transcripts. Both variants currently use
Noto Sans SC outlines, not separate regional Traditional Chinese glyph shapes.
The browser uses its installed fonts.

## Validation limits

The integration has local build and automated checks. Microphone capture,
speaker playback, cloud pairing and display stability during speech still need
validation on the actual Waveshare hardware. A successful build is not a voice
hardware test. Home Assistant tool execution and wake-word detection are not
implemented.

[Implementation](development/xiaozhi-ai.md) · [Waveshare panel](hardware/panel10.md)
