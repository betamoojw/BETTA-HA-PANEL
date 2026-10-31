<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Xiaozhi AI

Xiaozhi is an optional cloud voice assistant for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-10.1 (LCD-X, `panel10`). It is off by default.
Other board variants report voice audio as unsupported.

Configured [KNX communication objects](knx.md) are available to the assistant
through MCP when KNXnet/IP is enabled. Object permissions apply to voice commands;
the assistant cannot change KNX configuration or enable the interface.

## Device tools

The assistant advertises tools only on `panel10`, the profile with a supported
microphone and speaker:

- **Common Tools:** read device/network/display status, set assistant volume,
  set active display brightness, select a theme and query/control layout-bound HA entities.
- **KNX tools:** inspect configured objects and use their read/write permissions.
- **User Only Tools:** read firmware/system information, restart the panel,
  start an HTTPS firmware update and read display geometry.

Common and KNX tools are visible during normal tool discovery. User Only Tools
carry the MCP `audience: user` annotation and remain hidden until a
`withUserTools` discovery request opts into them for the current connection.
They are then callable until the MCP session is initialized again or
disconnected. This audience signal is a cloud protocol convention, not a second
local login prompt; use restart and firmware-update tools only from a Xiaozhi
client you trust.

Brightness, theme and volume changes use the same saved settings as the panel
and editor. Firmware URLs must pass the panel's existing HTTPS, image identity
and OTA compatibility checks. Camera capture, screenshot upload, image preview
and asset-download tools are not advertised because this profile has no matching
safe capability adapter.

Ask **"What is this panel's IP address?"** or **"Which Wi-Fi network are you
connected to, and how strong is the signal?"** The network tool reads the
panel's current Wi-Fi name, signal in dBm, channel, LAN IPv4 address, gateway,
netmask, IPv4 DNS servers, hostname and MAC address. Unavailable fields are
reported as unknown. It does not read passwords or change network settings;
a Wi-Fi connection alone does not prove internet access.

Ask **"Which Home Assistant entities are on this panel?"** or
**"What is the living room temperature shown on this panel?"** The assistant can
discover entities used by the saved layout and read their latest cached states,
units and selected attributes. HA state changes update this cache automatically;
adding or removing a layout binding changes what the assistant can query.
Only layout entities are shared with the paired Xiaozhi service when queried.
Offline, unsynchronized and unavailable states are identified explicitly.
Each new query reads the cache again; changes are not continuously pushed to the cloud.

Ask **"Through this panel's Home Assistant, turn on WLED-MTech"**, then
**"Set WLED-MTech brightness to 50 percent"** or **"Turn WLED-MTech off"**.
The assistant discovers the entity's supported actions before controlling it.
Lights, switches, fans, covers, climate controls, numbers, selects, buttons,
scenes and scripts have bounded actions; sensors and unsupported actions remain
read-only. Only entities in the current saved layout can be controlled.
Removing a binding removes access. Offline/unavailable entities and invalid
values are rejected. A sent command is not proof that a device changed: the
assistant must query HA feedback before claiming completion. HA configuration
and credentials cannot be changed through these tools. The COM24 trial verified
WLED power control and state feedback; other actions currently have local tests.

## Pairing

1. Open **Settings → Xiaozhi AI** in the web editor.
2. Enable the assistant, then select **Pair / check pairing**.
3. Sign in to [xiaozhi.me](https://xiaozhi.me) yourself and enter the code shown
   by the panel. Pairing retries expire after three minutes.
4. Wait for **Ready**. If pairing expired, start it again.

Pairing sends the device identifier and firmware information to the Xiaozhi
service. Audio goes to that service only during a conversation started with Talk
or a detected wake word. Optional wake-word
recognition runs locally; idle microphone samples are not uploaded.
It requires internet access and an account with the cloud service.

## Conversation

Tap the **AI agent (robot) icon** on the panel's top bar. Its frame, size and
colour follow the settings icon and the selected theme. The icon opens manual
controls, including **Hold to talk**, even when **Voice wake-up** is enabled.
It highlights while the dialogue is visible and returns to its normal colours
when closed, timed out or disabled, including dialogues opened by a wake word.
Hold the button, speak toward the microphone, then release to request a reply.
Opening the window alone does not start a cloud conversation.

With **Voice wake-up** enabled, say the saved wake phrase (default **Ni hao Xiao Zhi**, 你好小智), wait for
**Listening**, then speak. A voice wake opens the dialogue automatically with
the Talk button hidden. If the wake engine becomes unavailable, manual Talk
returns. Closing a voice dialogue and reopening it with the header icon returns
to manual controls. The browser offers Talk when wake detection is off or
unavailable; it uses the **panel microphone**. Capture ends after
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
and ends the cloud conversation, then returns to local wake detection. After a
wake word, automatic end-of-speech detection is used and the panel listens for
a follow-up after playback until the idle timeout. In manual mode, hold Talk
again for a follow-up. This is half-duplex: wake detection is
paused during a cloud conversation and speaker playback. Use the icon/close
button or Disconnect to cancel. Voice wake-up is independently off by default.
Press the top-bar button again or use **Disconnect** in the editor to end the
session. Disable the assistant to stop pairing, capture and the cloud connection.
Volume is limited to 80%; start low when first testing a connected speaker.

Controls save immediately except the wake phrase, which has its own Save button.
**Reset to defaults** disables
the assistant and voice wake-up, and restores 50% volume and a 30-second idle
timeout and Otto Robot style while retaining cloud pairing. Disabling
and re-enabling also preserves pairing, volume and emotion style. Pairing credentials remain
on this physical device; they are not included in ordinary layout/settings
exports and are never returned by the editor API.

## Custom wake phrase

Under **Settings → Xiaozhi AI**, enable **Voice wake-up**, choose **Custom Chinese
phrase**, and enter 3–12 lowercase pinyin syllables separated by spaces, without
tones (for example `ni hao guan jia` for 你好管家). Select **Save wake phrase** and
wait for **Wake phrase saved**. This changes actual local recognition; no model
training or firmware rebuild is needed when changing the phrase. Only Chinese
phrases are supported by the custom model. Choose a distinctive phrase and test
it in your room; sensitivity and false activations depend on speech and noise.
The bundled presets remain available: Ni hao Xiao Zhi, Hi ESP and Jarvis.

Install the matching `.wake-model.bin` release resource once using the file
picker shown for custom phrases. The panel checks its size and SHA256 before
using it. An interrupted upload can be restarted with the same file. The model
survives firmware updates and reset of assistant preferences. Without it, custom
phrases cannot be saved; presets and manual Talk remain available.

Saving ends the current conversation and validates the phrase locally. A rejected
phrase or storage failure keeps the previous saved choice. Disabling voice wake-up
or the assistant preserves the phrase. **Reset to defaults** restores Ni hao Xiao
Zhi and the custom draft `ni hao xiao zhi`, along with the other assistant defaults.
These device-local preferences are not included in layout/settings exports.

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
COM24 voice trials accepted status, volume, brightness, theme and network
queries (IP, Wi-Fi name and signal strength). These bounded checks do not
resolve the previously reported intermittent white flashes. Read-only Home Assistant
queries passed a COM24 voice trial: layout discovery, live on/off updates and
available brightness values. The owner confirmed replies, auto-close, touch and
normal output. This remains a bounded trial. A successful build alone does not establish
voice hardware support on another board.

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
