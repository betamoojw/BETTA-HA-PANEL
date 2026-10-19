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
