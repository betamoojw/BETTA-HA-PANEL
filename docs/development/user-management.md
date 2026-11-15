<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# User management validation

User-facing behavior and the HTTP contract: [Users](../users.md).

The design follows the four roles, bounded user count, password hashing and session revocation reviewed in
[edge_switch_actuator dev at 95e68f0](https://github.com/betamoojw/edge_switch_actuator/tree/95e68f098b49f470a8ae1540b534023ac750867e).
The panel implementation is original C. It uses opaque HttpOnly cookie sessions instead of that project's
JWT/localStorage flow. Relay channel masks do not map to this panel's shared dashboard. Physical touch,
HA, MQTT and KNX are outside the panel-account authorization boundary.

## Host checks

`tools/test_host.sh` runs the original admin-store test and the new account-store and HTTP lifecycle tests.
The latter include production storage, policy, guard and route handlers, with fake NVS/clock/HTTP transport.
They cover atomic-write failures, migration, password retention, disable/re-enable, last administrator,
role denial, sensitive settings filtering, sessions, expiry, revocation, cookie/origin checks and throttling.
Their password double tests account behavior, not cryptographic correctness.

For real password derivation, use the already installed ESP-IDF 6.1 crypto sources with a **host** compiler:

```sh
cmake -S tests/host/user_password -B /tmp/betta-password -DIDF_PATH="$IDF_PATH"
cmake --build /tmp/betta-password
/tmp/betta-password/test_user_password
```

On Windows supply a writable build directory and native C compiler, plus Ninja/CMake paths if needed;
run `test_user_password.exe`. This target compiles the real PSA implementation with a minimal SHA-256/HMAC
configuration. Three independently generated vectors exercise PBKDF2, UTF-8 passwords and legacy hashing,
plus constant-time comparison behavior, buffer wiping and unknown-algorithm rejection. It does not measure
target RAM, watchdog margins or timing.

Run `node --test tests/webui/*.test.mjs` and `python tools/check_web.py` for the browser source. Account tests
exercise form events through saved JSON/reload, role views, password clearing, fetch/OTA session handling
and all seven translations. Node DOM doubles do not establish visual layout or actual browser cookie behavior.

## Board acceptance

Build through the variant wrapper. Before flashing, collect existing version/tasks/log/reboot-log/coredumps
and back up the layout. After the owner-authorized flash, match the running source stamp and ELF hash to the
new image, confirm OTA validation and connection, and check the following with synthetic accounts:

- An unset device still opens; an existing `admin` password still works; no default account is invented.
- Viewer/operator/installer/administrator controls and direct API denials match the role table.
- Create/edit/disable/re-enable/delete persists after restart; blank password retains the credential.
- Current-password verification, logout, eight-hour expiry and account-edit revocation close access.
- Open Live Display/camera streams stop after revocation and lower roles cannot inject remote touch.
- Existing layout, themes, OTA upload, diagnostics and backups still work for administrators.
- Record HTTP task stack low-water and heap after those ordinary interactions. Do not substitute stress
  tests or destructive reset/OTA tests for authorized acceptance; never load a synthetic layout on a household panel.

Account provisioning changes access to the real device. Agree on owner credentials before creating the first
administrator; do not log credentials, cookies, or hashes. Keep device-specific results in ignored validation records.
