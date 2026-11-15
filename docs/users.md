<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Users

Open **Settings > System > Users**, or **Users** in the account bar. Panel accounts are separate from
Home Assistant users. They control access to the panel's browser and HTTP API.

## First administrator

A panel with no password retains open access after an upgrade. Create an enabled **Administrator**
to require sign-in. There is no default password. Anyone with network access can configure an open
panel, so complete this step on a trusted network.

An existing admin password remains valid with username `admin`. The first account edit migrates
that credential into the account database. Change its password to use the new password format.
Do not downgrade to firmware without user management: it does not understand the new account database
and may use an older password or allow open access.

## Roles

| Role | Browser | Additional API permissions |
| --- | --- | --- |
| Viewer | Refresh the panel screenshot; change own password | Read layout, entity states and display preferences; request a read-only Live Display session |
| Operator | Viewer features and switch the visible panel page | Navigate, open existing read-only tile popups and send notifications |
| Installer | Layout editor, entity search, room import, themes and read-only Live Display | Save layout, themes and custom translations; run existing display/sound/camera tests |
| Administrator | Full editor, settings and account management | Connection settings, firmware, restart/reset, camera configuration, KNX, voice assistant, diagnostics and crash dumps; remote touch |

The server checks permissions even when a client bypasses the browser controls. Unclassified API
routes require an administrator. Physical touch remains trusted. These accounts do not restrict
Home Assistant, MQTT or KNX operations: those integrations retain their existing credentials and controls.
Remote touch requires an administrator because it can reach the panel's native settings and actions.

## Manage accounts

- Up to **16 accounts**, including disabled accounts. Usernames are case-sensitive: 3–32 ASCII letters,
  digits, dots, underscores or hyphens. To rename someone, create the new account and remove the old one.
- New passwords use **8–64 UTF-8 bytes**. Chinese characters usually use three bytes each. Repeat the password.
  When editing an existing user, leave both password fields blank to keep the existing password.
- Disable an account to prevent sign-in while retaining its name, role and password. Re-enabling restores access.
- At least one enabled administrator must remain. The server rejects deleting, disabling or demoting the last one.
- **Every saved account change signs out all browsers**, including the administrator making the change.
  Save layout drafts first. Sign in again to reload the editor.
- **Change password** checks your current password. **Sign out** ends the current cookie session.
  Sessions expire eight hours after sign-in and do not survive a panel restart. Up to 16 sessions can be active.
- Accounts and password hashes are excluded from layout/settings backups. Factory reset erases them with other settings.
  If no administrator can sign in, recovery requires local USB access and erasing settings; back up data first where possible.

The local fixture editor does not create device accounts. Account editing is disabled there.

## API clients

`POST /api/auth/login` accepts JSON with `username` and `password`. The response sets an HttpOnly,
SameSite=Strict cookie; it never returns the session token in JSON. Reuse the cookie and add
`X-Betta-Request: 1` to POST/PUT/DELETE requests. JSON endpoints require `Content-Type: application/json`;
firmware upload retains its octet-stream content type. Browser mutations must originate from the panel.

| Endpoint | Purpose |
| --- | --- |
| `GET /api/auth/session` | Current sign-in state, username and role |
| `POST /api/auth/logout` | End the current cookie session |
| `POST /api/auth/password` | `current_password`, `new_password`; changes only the signed-in account |
| `GET /api/users` | Administrator-only redacted account list |
| `POST /api/users` | Administrator-only single-account edit |

For an account edit, send `action` (`create`, `update` or `delete`) and `username`.
Creation and update also require `password`, `enabled` (boolean) and `role`
(`viewer`, `operator`, `installer` or `administrator`). Successful changes return `reauthenticate: true`.
Invalid changes return 400; missing sign-in 401; insufficient role 403; throttling 429; unavailable storage 503.

HTTP Basic remains available for command-line tools, using any enabled account. Password checks have a
shared budget of 30 per minute; repeated Basic requests consume that budget. Prefer a cookie session for polling.
Six failed password checks from one address block that address for 60 seconds. Account edits also revoke
existing Live Display tickets and camera streams; active connections stop on their next checked operation.

This is plain HTTP, so passwords and cookies are visible to someone monitoring the network. Keep the panel
on a trusted LAN. See [Security](SECURITY.md) and [Security implementation](security-implementation.md).
