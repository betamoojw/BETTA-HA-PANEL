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

## v0.9.1 translation audit

Audit of `dev` at `728d3484` on 2026-11-01, with this wake-phrase correction:
all seven languages have exactly 2,031 browser keys and 732 panel keys, with no
missing or extra keys. Key completeness does not establish translation completeness.
The Spanish, French and Polish catalogs each had 12 English wake-phrase strings;
these are now translated, with the generated JavaScript updated alongside the JSON.
The offline-loader regression rejects English Xiaozhi instructions/status messages
while allowing the unchanged product name. Voice recognition code is unchanged.

Compared with the first parent of merge `d77b053`, the audited dev revision adds
454 browser keys, including later page-visibility and wake-phrase additions.
The following exact-English comparisons are review candidates, **not counts of
translation defects**: units, brands, technical identifiers and cognates can match.
The panel column compares the entire current panel catalog, not just new keys.

| Language | New browser values equal to English | Panel values equal to English |
| --- | ---: | ---: |
| English | Reference | Reference |
| German | 32 | 54 |
| Spanish | 374 | 195 |
| French | 375 | 204 |
| Polish | 372 | 197 |
| Simplified Chinese | 368 | 175 |
| Traditional Chinese | 368 | 175 |

Confirmed remaining English text in all five additional-language catalogs includes:

| Area | Browser keys to translate |
| --- | --- |
| Entity selection and validation | `entity_picker.rule_numeric`, `entity_source.invalid_summary_many`, `entity_source.price_help` |
| New widgets and Inspector help | `layout.widgets.add_stock_quotes`, `layout.inspector.stock_hint`, `inspector.help.widget_parqet_portfolio` |
| Device classes and previews | `device_class.signal_strength`, `preview.sensor.tomorrow`, `preview.weather.report` |
| Device and appearance settings | `settings.status.c6_hint`, `settings.theme.editing_id` |

Panel examples include `history.range_empty_12m`, `parqet.no_answer`,
`conn.retry_hint` and `conn.forbidden`. These are translation gaps, not missing
keys or evidence of a recognition failure. `preview.parqet.locale` also still
uses `en-US` in the five additional browser catalogs and needs locale review.
Keep technical values such as `XIRR`, `TTWROR`, `CO₂`, `PM2.5`, `UTC`, units,
entity IDs and executable YAML fields intact; string equality alone must not
trigger a replacement. The v0.9.1 release note about remaining English fallback
therefore still applies outside the corrected Xiaozhi controls.
