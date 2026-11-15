<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Security implementation

How the HTTP layer, authentication and update paths work and where they stop, in the current fork source.
Account storage, HTTP permissions and password vectors have host tests. These do not establish board-specific runtime
behavior or replace hardware validation. Existing paneljc evidence predates multi-user accounts; this is not a penetration test. Operating advice is on the
[Security](SECURITY.md) page, the historical comparison against v0.8.2 is linked from the [release notes](release-notes.md#v090).

## Exposure

- **Network:** the editor and API listen on HTTP port 80 of the panel's interface. The firmware sets up no UPnP or port
  forwarding and no cloud connection. Never forward port 80 or put the panel behind a public reverse proxy.
- **Without an admin password:** anyone on the network can open the editor, change every setting, see the screen, read the
  Home Assistant address and replace the firmware.
- **With accounts:** the login page and static files are public. Device data requires a viewer or higher role;
  configuration writes and diagnostics require the role mapped in `user_auth_policy.c`. Unmapped APIs require an
  administrator. Exact `GET /api/version` remains public; `/api/version?tasks=1` requires an administrator.
- **Credential storage unreadable:** protected routes answer `503` instead of opening. An unset password is a different
  state and leaves the panel open.
- **Transport:** plain HTTP. Anyone who can sniff the LAN sees layouts, screenshots and the Basic-auth password. Use a trusted
  LAN or an IoT VLAN, not a guest network.

## Endpoints open without a password

When accounts are enabled, the [Users role table](users.md#roles) applies to these endpoints.

| Endpoint | Allows | Secrets |
| --- | --- | --- |
| `GET/PUT /api/settings` | Read and change Wi-Fi, Home Assistant, MQTT, brightness, room | Passwords and the token are never returned, only "is set" flags; they can be overwritten |
| `GET/PUT/DELETE` layout, themes, `/api/i18n/*`, `/api/ha/light_entities` | Read and change dashboard, themes, texts | none |
| `POST /api/ota/url`, `/api/ota/upload` | Install firmware | no signature check |
| `POST /api/ota/check` | Ask the GitHub release API now, at most once a minute | none |
| `POST /api/notifications`, MQTT command topic | Full-screen message with an optional action and camera picture | none |
| `POST /api/factory-reset` with `{"confirm":"RESET"}` | Erase settings, layout and admin password | none |
| `GET /api/screenshot.jpg`, `/api/screenshot.png`, `/api/log`, `/api/state`, `/api/entities`, `/api/ha/*`, `/api/wifi/scan` | What the panel shows, its log, entity names and states, nearby networks | No credentials are returned on purpose, but screen and log can hold private data |
| `POST /api/restart`, `/api/navigate`, `/api/ui/tap`, `/api/sound/test`, `/api/camera/test`, `/api/coredump/erase` | Restart, navigate, open a read-only popup, test hardware, erase crash evidence | none; can disrupt operation |
| `GET /api/reboot-log`, `/api/coredump` | Restart history, stored panic dump | A dump can contain secrets from RAM: treat it as sensitive |

`POST /api/ota/auto-update-test` exists only in images built with `CONFIG_APP_AUTO_UPDATE_TEST_HOOK`, which no release uses.

## Protections

- **Accounts:** optional, at most 16, with viewer/operator/installer/administrator roles and an enabled flag. One enabled
  administrator must remain. A versioned byte-only NVS blob is written atomically. Read or validation errors fail closed.
  New passwords use a random 16-byte salt, PBKDF2-HMAC-SHA256 (10,000 rounds), and a 32-byte hash. Existing iterated SHA-256
  admin credentials are accepted until that password is changed. Accounts and hashes never enter HTTP exports.
- **Sessions:** 32 random bytes encoded as a cookie, HttpOnly and SameSite=Strict, eight-hour absolute monotonic lifetime,
  at most 16 sessions. Restart or any account edit revokes all sessions. Logout revokes that session; async streams and
  remote display tickets carry a generation/expiry lease so they cannot continue after revocation. Passwords/tokens are
  not stored in browser Web Storage. Basic authentication remains supported. The HTTP task owns account/session state;
  async workers validate immutable leases against atomic generation counters.
- **Password throttling:** six failed checks block an address for 60 seconds; 16 addresses are tracked. A separate
  global budget limits password verification to 30 checks per minute, including valid Basic requests. Cookie reads avoid
  repeated derivation. Existing generic request limits still apply.
- **Request checks** (`main/api/http_guard.c`): POST and PUT need `Content-Type: application/json` (`application/octet-stream`
  for the firmware upload) or answer `415`, so a browser form or `text/plain` request from another site is refused. There is
  no CORS answer. Mutations with an Origin must match `http://Host`; cookie-authenticated mutations also need `X-Betta-Request: 1`. DELETE relies on this check and authentication. API requests are limited to 12 per second with
  a burst of 24 per client, 3 parallel GET and 1 parallel other request per client, and 4 in flight overall. Bodies are
  bounded per endpoint (layout 32 KB, settings 4 KB, notifications 2 KB).
- **Response headers:** `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff` and `Referrer-Policy: no-referrer` on
  guarded responses; the editor page adds a Content-Security-Policy (`script-src 'self'`, no inline script, no eval,
  `frame-ancestors 'none'`, connections only to the panel and `api.github.com`). Styles may be inline (`'unsafe-inline'`).
  Preview text from widget titles and Home Assistant states is escaped.
- **Secrets and addresses:** changing `ha.ws_url` or `mqtt.broker_host` needs the matching token or password in the same
  request (`400` otherwise), so a stored secret is never sent to a new address. This is a string-change guard, not
  certificate pinning.
- **Home Assistant token scope:** only URLs on the Home Assistant origin carry the token; cover and camera images from other
  hosts are fetched without it and redirects are not followed when it is sent (`cover_url_on_origin`). PNG chunk lengths are
  bounds checked and large images skipped.
- **Rejected token:** a rejected Home Assistant token is not retried until a restart, so the panel cannot run into Home
  Assistant's IP ban.
- **Notification actions:** the action id must be a plain entity id in `button`, `input_button`, `lock`, `scene`, `script`,
  `switch`, `light`, `input_boolean` or `fan`; anything else is dropped and the card shows without the button. Layout alerts
  follow the same rule. A lock can be unlocked or opened only through the `unlock` or `open` service.
- **Firmware updates:** an update must be an ESP app image of the same project name and fit the next slot. A release installed
  through the update check is compared with the SHA-256 GitHub publishes (integrity, not authenticity), and a new image must
  mark itself valid or the bootloader rolls back. A custom URL must be HTTPS (certificates checked against the built-in CA
  bundle, redirects stay on HTTPS); an upload or custom URL gets no digest or signature check. Automatic installation needs a
  known digest and a rollback-capable bootloader and is off by default.
- **Language files:** stored under a validated code (`[a-z0-9_-]`), so a name cannot leave its directory.

## Known limits

| Limit | Impact | What to do |
| --- | --- | --- |
| Firmware is not signed, no secure boot | An administrator (or anyone while access is open) can flash any image of the same project name | Set the password. Secure boot v2 is a one-way eFuse change and needs a recovery plan first |
| Secrets sit in NVS in plain text, no flash encryption | A stolen panel gives away the Home Assistant token, Wi-Fi and MQTT passwords | A dedicated Home Assistant user with a revocable token; rotate it when a panel is lost |
| PBKDF2 uses 10,000 iterations; migrated legacy hashes remain iterated SHA-256 until changed | Offline guessing remains possible after flash extraction | Use long unique passwords and change the migrated administrator password |
| Setup access point `BETTA-Setup-XXXX` uses a random two-word, two-digit WPA2 passphrase (about 27 bits, shown on the display) | A recorded handshake within radio range can be attacked offline; the editor on that AP needs no password unless one is set | Do the first setup out of reach of strangers and set the password early; the AP exists only while no Wi-Fi is configured or the saved network is unreachable; the passphrase is new at every start |
| `GET /api/version` is public | Board, component versions, memory, uptime, restart reason, Wi-Fi diagnostics and crash-dump metadata, no credentials or dump | Keep the panel off untrusted networks |
| No fixed Host allowlist or HTTPS | Origin/Host matching and the request marker do not provide transport security or DNS-rebinding protection for an open panel | Enable accounts and restrict network access |
| Custom language files are partly inserted as HTML | The CSP blocks inline script and handlers but does not sanitise markup or misleading links | Treat language files as trusted configuration |
| Notification cards can carry an action | Without a password a LAN attacker can show a convincing "doorbell" card with an unlock button; the action runs when the card is tapped | Set the password; protect the MQTT broker with a login and ACLs |
| MQTT without TLS | Broker login and messages in clear text | Enable `mqtt.tls_enabled` with a trusted certificate; limit publishing to the command topic with ACLs |
| `ws://` Home Assistant connection | The token crosses the LAN in clear text; `wss://` verifies the built-in CA bundle only, no self-signed certificates | Use `wss://` with a trusted certificate where the LAN is not trusted |
| Login lock-out tracks 16 addresses, plus a global password-check budget | Attackers can consume the shared budget and delay sign-in | Restrict LAN access; use cookie sessions for API polling |
| A permanently damaged credential record | A new password cannot be set over HTTP | Erase the settings over USB (`esptool erase-flash` or the NVS partition) |
