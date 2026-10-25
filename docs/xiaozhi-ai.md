<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi AI

Xiaozhi is an optional cloud voice assistant for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1 (LCD-X, `panel10`). It is off by default.
Other board variants report voice audio as unsupported.

Configured [KNX communication objects](knx.md) are available to the assistant
through MCP when KNXnet/IP is enabled. Object permissions apply to voice commands;
the assistant cannot change KNX configuration or enable the interface.

## Pairing

1. Open **Settings → Xiaozhi AI** in the web editor.
2. Enable the assistant, then select **Pair / check pairing**.
3. Sign in to [xiaozhi.me](https://xiaozhi.me) yourself and enter the code shown
   by the panel. Pairing retries expire after three minutes.
4. Wait for **Ready**. If pairing expired, start it again.

Pairing sends the device identifier and firmware information to the Xiaozhi
service. Audio goes to that service only during a conversation started with Talk, the AI
icon in voice wake-up mode, or a detected wake word. Optional wake-word
recognition runs locally; idle microphone samples are not uploaded.
It requires internet access and an account with the cloud service.

## Conversation

Tap the **AI agent (robot) icon** on the panel's top bar. Its frame, size and
colour follow the settings icon and the selected theme. Hold **Talk**, speak toward the
microphone, then release to request a reply. With **Voice wake-up** enabled, say
**Ni hao Xiao Zhi** (你好小智), wait for **Listening**, then speak. The dialogue
opens automatically; the AI icon can also open it and start listening. The
Talk button is hidden while the local wake engine is ready. If it cannot start,
manual Talk remains available. The browser's Talk button also
uses the **panel microphone**, not the computer microphone. Capture ends after
at most 60 seconds even if the browser disappears.

The reply plays through the panel speaker. A centered emotion follows the
assistant response, with separate recognized-speech and AI-reply text below it.
The existing status and pairing code remain at the top. The popup follows the
system palette and appearance; long transcripts scroll inside it.

**Otto Robot** is the default emotion style, with 21 animated expressions.
The original **Classic Emoji** remains available. Select **Emotion style** in
the editor's **Settings → Xiaozhi AI** page, or **AI emotion style** in the
panel's **System settings**. Web changes save immediately; panel changes apply
when you select **Save**, and **Cancel** keeps the previous choice. Both controls
use the same saved preference. Changing the style does not enable the assistant
or microphone. The Otto face keeps its black background inside the themed popup.

**Dialogue idle timeout** is 30 seconds by default (10–300 seconds). Recognized
voice, received speech text and reply audio renew it. Silence closes the popup
and ends the cloud conversation, then returns to local wake detection. Cloud
replies use automatic end-of-speech detection; after playback the panel listens
for a follow-up until the idle timeout. This is half-duplex: wake detection is
paused during a cloud conversation and speaker playback. Use the icon/close
button or Disconnect to cancel. Voice wake-up is independently off by default.
Press the top-bar button again or use **Disconnect** in the editor to end the
session. Disable the assistant to stop pairing, capture and the cloud connection.
Volume is limited to 80%; start low when first testing a connected speaker.

Controls on this settings page save immediately. **Reset to defaults** disables
the assistant and voice wake-up, and restores 50% volume and a 30-second idle
timeout and Otto Robot style while retaining cloud pairing. Disabling
and re-enabling also preserves pairing, volume and emotion style. Pairing credentials remain
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
interface text. On Waveshare P4, the optional local transcript font expands
recognized speech and replies to 29,417 Chinese/punctuation/source characters.
Install it once with the [resource installer](development/xiaozhi-resources.md#local-transcript-font);
later app updates retain it. Without this resource, characters outside the compact
subset still show placeholders. Both variants currently use
Noto Sans SC outlines, not separate regional Traditional Chinese glyph shapes.
The browser uses its installed fonts.

## Validation limits

The Waveshare COM24 trial exercised saved cloud pairing, manual speech,
hands-free wake/reply, both emotion choices and idle auto-close. The owner
confirmed Otto animation and stable physical output during speech. These are
bounded device checks: far-field sensitivity, false-activation rates, speaker
feedback in other rooms and all-day operation have not been measured.
Home Assistant tool execution is not implemented. A successful build alone
does not establish voice hardware support on another board.

[Implementation](development/xiaozhi-ai.md) · [Waveshare panel](hardware/panel10.md)

On 2026-10-24, the AI agent header icon build (`a4eeaa88-dirty`, app SHA256
`514afdf107d81d5b5e8b6ce5c701f6e413329e83e9e186277ccb0b1b0dbcba79`)
passed an app-only COM24 update with the supported P4 stub. Application readback
matched; metadata/NVS, crash storage and complete LittleFS checksums were unchanged
before boot. The running ELF matched and OTA was valid. Remote clicks opened and
closed the assistant, and screenshots showed matching icon frames without overlap
at 1280 x 800 and 800 x 1280. The original 90-degree portrait setting, English,
latest settings, layout and themes were restored and compared. This was a header
UI test, not a cloud speech or audio acceptance test.

The owner confirmed the icon and click behavior, portrait layout, stable physical
output and aligned touch. Monitoring covered 31 samples over 454 seconds without
an unexpected restart or new crash. After restoration, the owner intentionally
selected 180 degrees and asked to retain it. Final comparison preserved that
choice and English; layout differences were only orientation projection.
