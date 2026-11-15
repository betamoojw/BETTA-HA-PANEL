<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# License management

Open **License** in the web editor's account bar to view the device ID, license
state and trial time. Every signed-in role can read this information; an
administrator can import or remove a license. With no accounts configured,
the editor retains its existing open administrator access.

License status is **informational only**. All features remain available without
a license and after the 60-minute trial ends. License state never causes a
restart, brightness change, audio interruption or control restriction.
The trial counts uptime from each boot; importing or removing a license does
not restart it.

## Import a license

1. Copy the **Device ID** and send it to your supplier.
2. Obtain the supplier's signed `DEVICE_KEY` file for this panel.
3. Choose the file and select **Import license**.
4. Confirm **Licensed permanently**. Refresh checks the saved state again.

A malformed file, another device's file or a file signed by another supplier is
rejected without replacing the existing license. A lost connection during import
may hide a successful save; reconnect and refresh before trying again.

**Issuer public key is not configured** means the firmware builder must first
configure your supplier's public key. Import is unavailable until then; ordinary
panel use is unchanged. Older WLED license files are not compatible.

**Remove license** deletes only the saved license after confirmation. It does not
change users, dashboards or settings. License data survives ordinary restarts
and OTA updates and is excluded from layout/settings backups. A full flash erase
clears it. Keep the original signed file for reimport. Changing the issuer key
requires licenses signed by the new issuer.

English, German and Simplified Chinese license labels are translated; other
editor languages currently use English for this dialog.

For builders: [Issuer setup and signature verification](development/licensing.md).
