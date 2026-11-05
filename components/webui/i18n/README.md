<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk | Modified 2026 by andre68723 -->
# Translation catalogs

`es.json`, `fr.json`, `pl.json`, `zh-cn.json` and `zh-tw.json` are editable browser catalogs. Panel catalogs
live in `main/i18n/`. The initial translation sources came from
`betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3` commit
`4e117ca03c05e7f6ef96de2e435cb24da7c785e7` (Copyright 2026 Cpt_Kirk,
LicenseRef-FNCL-1.1); current keys and wording were extended in this fork.

Spanish, French and Polish also reuse compatible translations from reference
`dev` revision `db7641566be6b8d36d501154f9c49211ef365367`, extended for all current keys.
The existing English/German sources remain in `www/js/i18n.js` and
`main/settings/i18n_store.c`.

Run `python3 tools/generate_i18n.py` after editing either set. Commit the
JSON and generated JS/INC files together. `--check` detects stale outputs;
`node --test tests/webui/chinese_i18n.test.mjs` checks key coverage, interpolation,
printf argument order and CJK glyph coverage. Build and runtime need no network
translation service. Preserve placeholders and HTML/code examples when translating.

Firmware uses the shared runtime font manager. If a new panel translation needs
a character absent from `main/ui/fonts/cjk_codepoints.txt`, regenerate the
manifest and Noto subset as described in `main/ui/fonts/README.md`.
Changing a translation never changes a user's saved title or entity name.

## v0.9.1 translation coverage

All seven languages have 2,031 browser keys and 732 panel keys. The Spanish,
French, Polish, Simplified Chinese and Traditional Chinese catalogs now localize
the v0.9.1 additions: entity selection and source validation, stock/portfolio and
price widgets, device classes, previews, memory diagnostics, appearance, history,
connection feedback and printer states. The wake-phrase correction adds 12
Spanish, French and Polish strings per language, including offline feedback.

Compared with the wake-phrase correction at `ac8f6197`, this follow-up changes:

| Language | Browser values | Panel values |
| --- | ---: | ---: |
| Spanish | 354 | 160 |
| French | 345 | 162 |
| Polish | 348 | 160 |
| Simplified Chinese | 360 | 168 |
| Traditional Chinese | 360 | 168 |

The portfolio preview uses es-ES, fr-FR, pl-PL, zh-CN and zh-TW number locales.
Context corrections distinguish current data from electrical current, minimum
prices from minutes, stock variation from an edit action, and low battery states
from minimum values.
Technical keys, interpolation fields, printf argument order, entity IDs and
executable YAML examples remain intact. No recognition, networking or saved-data
logic changes. The shared UI font subset adds 115 characters for translated
panel labels; it contains 917 codepoints and retains the existing glyphs.

`tests/webui/i18n_shared_values.json` records the reviewed English-equal values:
product/protocol names, units, domain identifiers, native language names, code
examples and valid shared words such as French "Mode" or Spanish "Error".
The existing catalog tests now reject other English-equal browser and panel
values. This detects copied English placeholders in new labels; it does not
replace semantic review or detect every partially translated sentence. Do not
expand the allowlist merely to make a new untranslated label pass.
