# Timezone database

Source: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.json

Downloaded 2026-09-23 together with the upstream MIT LICENSE.
SHA-256 of zones.json:
`b95662f059d0bf1408962272cb98da4820fa0e20c7582e0aca1d0613e33986ef`

`zones.json` is the source of truth; run `python tools/generate_timezones.py`
after updating it. Commit both JSON and generated `zones.inc`. Firmware uses
the const table directly without parsing JSON or fetching external resources.
The existing settings API supplies the same names to the web editor.

These are recurring POSIX rules, not a full historical IANA database. Update
the snapshot when regional rules change. Multiple names intentionally share
rules; the selected name must never be inferred from the rules alone.

The hash above describes the imported snapshot before local edits.
Five Canadian locations use forward-looking fixed offsets from
[IANA tzdata 2026b through 2026e](https://data.iana.org/time-zones/tzdb/NEWS):
Vancouver UTC-07; Edmonton, Inuvik and Yellowknife UTC-06; Winnipeg UTC-05.
The recurring rules cannot represent the historical transition dates.

## Regional date formats

`date_patterns.txt` groups every catalog location by territory and numeric
Gregorian date pattern. The firmware table and the editor clock API use this
same snapshot. It uses IANA's single-territory `zone.tab` with historical names
resolved through `backward`. Numeric fields follow the territory's likely
locale from Unicode CLDR. The panel uses Latin digits, padded days and months,
a four-digit year and one numeric separator. UTC, fixed offsets, Antarctic
stations without a territory convention, and location-free POSIX rules use
`Y-M-D`. [IANA tzdata 2026a](https://data.iana.org/time-zones/releases/tzdata2026a.tar.gz)
and [Unicode CLDR date formats](https://unicode.org/reports/tr35/tr35-dates.html).
CLDR data is © 2004–2025 Unicode, Inc. See the [Unicode license](CLDR-LICENSE.txt).

For a data refresh, unpack `zone.tab`, `backward` and `version` from the
IANA archive and run `node tools/update_timezone_dates.mjs <unpacked-directory>`
with full ICU (this snapshot used Node 24.15.0, CLDR 48.0). Then run
`python tools/generate_timezones.py`. Normal builds and checks are offline.
