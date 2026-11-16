<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# License management

Open **Settings → System → License** in the web editor to view the device ID,
license state and trial countdown. Every signed-in role can read this information; an
administrator can import or remove a license. With no accounts configured,
the editor retains its existing open administrator access.

License status is **informational only**. All features remain available without
a license and after the 60-minute trial ends. License state never causes a
restart, brightness change, audio interruption or control restriction.
The trial counts uptime from each boot; importing or removing a license does
not restart it.

A black-and-yellow warning strip appears at the top of login, initial Wi-Fi/HA
setup, editor settings/layout, Live Display and viewer pages when no valid license
is available. It names missing or invalid licenses, verification/storage errors
and an unconfigured issuer separately. If a status request fails, it says the
status is unavailable rather than claiming the license is invalid.

The strip is informational and cannot be dismissed. Its reserved space follows
wrapped text on small screens, keeping controls below it. A valid license removes
both the strip and its reserved space. Activation/removal updates it immediately;
other tabs refresh periodically while visible. All seven editor languages are
supported. Before sign-in, the browser language is used. Local editor previews
do not claim or request a device license.

## LCD warning after 30 minutes

After 30 minutes of uptime, a translucent black/yellow warning overlays the LCD
top bar when the license is missing or invalid, the issuer is not configured,
or its status is unavailable. Each condition has its own message. Small screens
and enlarged text may show a shorter license warning. All seven panel languages
are supported. The 60-minute informational trial and immediate web warning keep
their existing timing.

The overlay partly covers left-side date, weather or stock information. It adds
no top spacing and leaves the clock, connection controls and settings button
usable. It also appears in the native settings header and persistent Wi-Fi/HA
setup screen. Fullscreen dialogs and overlays keep priority; screensavers and
screen-off modes suppress the warning without being woken by it.

A valid activation hides the warning on the next display update. Removing a
license after 30 minutes shows it again. Only reboot restarts the delay; page
changes, network reconnection, language changes and clock adjustments do not.
The warning never restricts features, changes brightness or writes settings.

## Import a license

1. Copy the **Device ID** and send it to your supplier.
2. Obtain the supplier's signed `DEVICE_KEY` file for this panel.
3. Paste the signed key or choose the file, then select **Activate license**.
4. Confirm **Licensed permanently**. Refresh checks the saved state again.

**Copy device ID** supports the panel's local HTTP page as well as secure browser
contexts. If browser permissions prevent copying, the complete ID is selected
and a manual-copy message appears; use your browser's Copy action or Ctrl+C/Cmd+C.
Copying an ID never imports a license or changes panel settings.

A malformed file, another device's file or a file signed by another supplier is
rejected without replacing the existing license. A lost connection during import
may hide a successful save; reconnect and refresh before trying again.

**Issuer public key is not configured** means the firmware builder must first
configure your supplier's public key. Administrators can still choose a file or
paste a key locally; selection alone does not upload or activate it. **Activate
license** stays disabled with an explanation until compatible firmware is installed.
Ordinary panel use is unchanged. Older WLED license files are not compatible.

**Remove license** deletes only the saved license after confirmation. It does not
change users, dashboards or settings. License data survives ordinary restarts
and OTA updates and is excluded from layout/settings backups. A full flash erase
clears it. Keep the original signed file for reimport. Changing the issuer key
requires licenses signed by the new issuer.

Selecting a file clears the pasted key; typing a key clears the file selection.
Cancelling the file picker preserves a pasted key. Refreshing status retains the
selected file or pasted key.
After activation the key entry is hidden; **Refresh** and **Remove license** remain
available. Leaving this settings page clears the unsent key. Viewer and operator
accounts open **Settings** from their panel-screen page; only License is available.

## Agreement and reading record

The **Agreement** tab contains the repository's complete `LICENSE`, including
its third-party component notices. This text applies independently of device
activation. A device key does not grant additional software rights or replace
the separate written permission required for commercial use.

**I have read this license and its third-party notices** records the reading
time in this browser, separately for each device, account and LICENSE revision.
The checkbox is optional; it does not activate a license or block panel use.
Uncheck it to remove the record. Reloading retains it, but another browser,
cleared browser data or a changed LICENSE requires a new local confirmation.
Browser storage errors are shown without reporting a successful confirmation.
No reading record is sent to the device, supplier or another service. The local
editor preview displays the agreement but cannot save a device reading record.

Labels, status, feedback and reading dates follow the editor language in English,
German, Spanish, French, Polish, Simplified Chinese and Traditional Chinese.
The page's file-selection button and empty-file hint also follow the editor language;
the system file dialog uses the browser or operating system language.
Reading records remain unchanged when the language changes.
The LICENSE text remains in its original English to preserve the actual terms.

For builders: [Issuer setup and signature verification](development/licensing.md).
