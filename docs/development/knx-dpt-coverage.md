<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# KNX datapoint coverage

See [KNX development and checks](knx.md#checks) for how to run the tests.

The capacity regression exercises 512 distinct groups, the last HTTP/MCP pages,
last-object feedback and a write, and rejection of a 513th object. The 352-row
reference in `tests/knx/KNX-Datapoint-Types.md` includes 264 subtypes accepted
by the current codecs; the fixture covers those supported reference types.
These 88 listed types remain unsupported and must not be added to the valid fixture
until their typed codecs and validation rules are implemented:

~~~text
200.100 200.101
201.100 201.102 201.104 201.105 201.107 201.108 201.109
202.001 202.002
203.002 203.003 203.004 203.005 203.006 203.007 203.011 203.012 203.013 203.014
203.015 203.017 203.100 203.101 203.102 203.104
204.001
205.002 205.003 205.004 205.005 205.006 205.007 205.017 205.100 205.101 205.102
206.100 206.102 206.104 206.105
207.100 207.101 207.102 207.104 207.105 207.600
209.100 209.101 209.102 209.103
210.100 211.100 212.100 212.101 213.100 213.101 213.102
214.100 214.101 215.100 215.101 216.100
217.001 218.001 218.002 219.001 220.100 221.001
222.100 222.101 223.100 224.100 225.001 225.002 225.003
229.001 230.1000 231.001 235.001 236.001 237.600
238.001 238.600 239.001 240.800 241.800
~~~

With Node installed, the `knx_editor_registry_import` CTest runs the editor's
import and reload against the native KNX service. It checks the stored JSON,
installed object count and each object's DPT after a successful save and after
rejected edits. The standalone web suite also round-trips the shipped file.

## Panel10 editor import on COM24, November 28, 2026

The Waveshare 10.1-inch LCD-X ran the Windows `dev` workspace build
`b09ddf04-dirty` (ESP-IDF 6.1.1, editor build time **17:05:15**, UTC+8).
The serial log reported **17:05:11** for the ESP-IDF application descriptor
and ELF hash prefix `c739e73fe`, matching the archived build.
The application occupied 6,731,584 bytes and left 84,160 bytes in each OTA slot.
The pre-flash editor backup was checked against the authenticated device APIs.
The flash backup was mounted offline to check the layout and the original 76 objects.

Only the running `ota_0` application at `0x20000` was written. All 6,815,744
slot bytes were read back over COM24 and matched the new application and original
tail. Checksums of the inactive slot, metadata (including NVS and OTA selection),
crash partition and LittleFS matched before and after writing, before reset.

| Artifact | SHA-256 |
| --- | --- |
| Application | `d92c991e77631c8e0a53de1ebf6084e74a5b7e155198135bfeac6a16401ea0d5` |
| Full `ota_0` readback | `936dff63d35e3da40f02d3a2df44051efe5d49b467c0fee9c1aec0f56606e75e` |

Chrome loaded the editor directly from the panel. Importing the repository's
original `tests/knx/knxConfig.json` left the stored 76 objects intact until
**Save configuration**. The save returned HTTP 200 and displayed **Configuration
saved**. **Reload saved** and a newly opened editor both showed all 302 objects.
The saved JSON matched every field in the fixture. The direct configuration GET
and the editor's reload GET returned identical bytes with SHA-256
(`262606f22935ffda3fb95b9fc921a9391aaf2d5c5164b1431394301bb8956939`).
All 16 pages of the running registry matched the fixture's IDs, names, group
addresses, DPTs and permissions. The DPTs below were checked in the reopened
editor's JSON and in the running objects' `datapoint_type` fields.

| Object | Group address | DPT in the reopened editor and running registry |
| --- | --- | --- |
| `dpt_7_010` | `11/7/21` | `DPT-7.010` |
| `dpt_20_1000` | `11/7/93` | `DPT-20.1000` |

The second save restored all 76 original objects. All four pages of the running
registry matched the original objects. The saved registry readback matched
the pre-flash bytes (SHA-256 `07ed6611a32e06e4f52dd65c094b4b1192e9dece712d63a578d8981b86bc1b4b`).
These two registry saves were the editor's only writes. The final editor backup
matched the original except for its export time. Both **Light Angle** and
**Power history** kept their bindings in the 10-page, 61-widget layout. The
LittleFS backup contained no saved graph samples for a point-by-point comparison.
Anonymous layout access returned HTTP 401 after the test session signed out.

The device reported boot 276, valid `ota_0` and idle OTA through the final
check. The 112-second serial capture contained
one boot and no panic. Two JPEG decoder errors preceded the import. This run
verified editor reload within the same boot. It did not exercise camera playback,
a cold boot with 302 objects, or physical KNX actuators. Raw serial, flash
checksums, browser requests and device readbacks are archived locally in
`.local/validation/panel10-knx-302-com24-20261128/`.
