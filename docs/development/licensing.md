<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Device licensing

User workflow: [License management](../license-management.md).

## Reference and scope

Reviewed against [betamoojw/WLED_UserMods UserModsDev, 3dd35f4](https://github.com/betamoojw/WLED_UserMods/tree/3dd35f4ee9c684f27316a33fd51253d66852a74c/usermods/License_Mgnt)
and [`wled00/util.cpp`](https://github.com/betamoojw/WLED_UserMods/blob/3dd35f4ee9c684f27316a33fd51253d66852a74c/wled00/util.cpp).
That implementation exposes a device ID, imports `DEVICE_KEY`, reports permanent
activation and runs an in-memory trial. Its public SHA-1 derivation and fixed salt
allow anyone with the ID to calculate a valid key. The nominal minute tick starts
at 61 seconds because its comparison uses elapsed whole seconds greater than 60.
Trial expiration schedules a red blink and restart; pending restart runs before
the next validation, even if a key was imported meanwhile.

BETTA implements the workflow independently with signatures. At the owner's
request, status is **informational only**. HA, KNX, MQTT, camera, audio, LVGL,
remote input and control permissions do not consult the license. The trial uses
exactly 3,600 seconds of monotonic uptime per boot. There is no enforced trial,
calendar expiry, online activation or revocation list. This runtime license does
not replace the repository's source license.

## Identity and format

All variants compile the shared service. The device ID is SHA-256 of ASCII
`BETTA-HA-PANEL/v1:` plus the application chip's factory MAC as 12 uppercase hex
digits, returned as 64 lowercase hex characters. `esp_efuse_mac_get_default()`
uses the P4 identity, not its C6 radio. Room, flash, calibration and firmware
settings do not participate.

The signed file is exactly 200 ASCII bytes:

```text
BETTA1:<64 lowercase hex device ID>:<128 lowercase hex signature>
```

One trailing LF or CRLF is accepted and omitted in storage. NUL, uppercase hex,
suffixes, spaces and other versions are rejected. The signature is ECDSA P-256
with SHA-256, encoded as 32-byte `r` followed by 32-byte `s`. Signed bytes are
`BETTA-HA-PANEL-LICENSE/v1\n<device ID>\nperpetual\n`, where `\n` means an LF byte.
Domain, identity and entitlement are signed. PSA verifies with a public key only.
An unmodified verifier authenticates the issuer; someone controlling source or
flashing can replace the verifier. Old WLED keys are deliberately incompatible.

## Issuer setup

Use the existing ESP-IDF Python environment or the locked `checks` dependency
group, which includes `cryptography`. Keep private keys outside the repository
and restrict filesystem ACLs; POSIX mode 0600 does not configure Windows ACLs.

```bash
python tools/license_issuer.py keygen --private-key /secure/issuer.pem --public-key /secure/issuer-public.pem
python tools/license_issuer.py public --public-key /secure/issuer-public.pem
```

Key generation prompts for a passphrase and refuses to overwrite either file.
The public command prints `CONFIG_APP_LICENSE_PUBLIC_KEY_HEX="..."`, a 130-character
uncompressed SEC1 P-256 key starting with `04`. Add this public value to private
build defaults or configure it through menuconfig using the variant wrapper.
Reconfigure and build the intended variants. The default is empty: no production
or test issuer is trusted by this repository.

```bash
python tools/license_issuer.py sign --private-key /secure/issuer.pem --device-id <copied-device-id> --output DEVICE_KEY
```

Signing prompts for an encrypted key's passphrase and refuses to overwrite output.
Nothing is sent over a network. Back up the issuer key securely. Changing the
firmware public key requires reissuing licenses under the new key.

## Storage and API

Initialization runs after NVS and before task startup. `betta_license/device_key`
holds one fixed-size blob. Import verifies before writing and reports success
only after `nvs_commit`. Validation failure preserves the existing blob. A failed
write/commit reports an error because durability is uncertain. Remove touches
only that key and never restarts the trial. All NVS and PSA handles are released.

`GET /api/license` requires viewer access. `POST /api/license` imports raw bytes
with `Content-Type: application/octet-stream`; `DELETE /api/license` removes them.
Mutations require administrator access plus existing same-origin/CSRF checks.
Responses are not cached and never contain the signature. `/api/version`
advertises `license_status_only: true` without publishing identity or license data.
General settings/layout backups do not contain licenses.

There is no service task, periodic NVS write or display lock. Status reads use
atomic state and uptime. Verification occurs at boot or on the serialized HTTP
import path. Upload memory is bounded to 202 bytes plus crypto internals.
The browser reads metadata once per minute while visible, with a ten-second
deadline; it does not request screenshots or operate the panel.

## Validation

`tools/test_host.sh` runs storage fault injection and real HTTP role/CSRF tests.
Its lifecycle test substitutes crypto. The separate PSA target verifies actual
signatures against ESP-IDF 6.1 software crypto:

```bash
cmake -S tests/host/user_password -B build/host-crypto -DIDF_PATH="$IDF_PATH"
cmake --build build/host-crypto --target test_license_crypto test_user_password
build/host-crypto/test_license_crypto
build/host-crypto/test_user_password
node --test tests/webui/*.test.mjs
python -m unittest discover -s tests/tools -p test_license_issuer.py
python tools/check_web.py
```

The public test vector corresponds to known scalar 1; never use that test issuer
in production. It covers Python-to-C interoperability, identity binding, 200
byte mutations and 100 repeated verifications. Browser tests exercise import,
reload, removal, permissions, malformed files and failed saves.

Host success does not qualify device crypto latency, heap peaks, power-loss
durability or unchanged display/audio timing. Measure those after a separately
authorized installation on each intended board; local tests require no device.
