# Agent prompt: Enumerate all KNX Datapoint Types

## Objective

Update `tests/knx/knxConfig.json` so that `comObjects` provides practical, real-world
coverage for every KNX Datapoint Type listed in
`tests/knx/KNX-Datapoint-Types.md`.

This is an implementation task, not a documentation-only task. Work in the repository,
preserve existing configuration, modify only the required JSON file unless a validation
failure requires a tightly related fix, and verify the result.

## Required inputs

Read and use all of the following:

- `tests/knx/KNX-Datapoint-Types.md`
- `tests/knx/knxConfig.json`
- `docs/knx.md`
- `docs/development/knx.md`
- `main/knx/README.md`
- The KNX object parser and codec tests under `tests/knx/`
- `tests/knx/CMakeLists.txt`

Use the source reference only for Datapoint Type names and semantics. Do not invent a
real KNX installation, physical device, or unsupported unit. Practical examples must
represent plausible building-control, metering, HVAC, security, lighting, shading,
access, audio, or maintenance use.

## Existing configuration rules

Keep the top-level keys and `media_parameters` of `knxConfig.json` unchanged.

Preserve every existing entry in `comObjects` unless it has a concrete semantic or
structural problem. Prefer adding entries to complete coverage over deleting existing
ones.

Retain the existing object shape:

```json
{
  "id": "unique_snake_case_id",
  "name": "Human-readable name",
  "description": "Practical purpose and value semantics",
  "group_address": "main/middle/sub",
  "datapoint_type": "DPT-Major.Minor",
  "readable": true,
  "writable": true,
  "unit": null
}
```

Rules:

- Use a stable, unique, lowercase `snake_case` ID containing only letters, digits, and
  underscores.
- Use a unique three-level KNX group address for every object.
- Do not reuse a group address for independent new objects.
- Existing command/status pairs may continue sharing a group address only when that is
  already the established pattern or is semantically intentional.
- Convert the reference identifier exactly into the JSON form `DPT-Major.Minor`, for
  example `1.001` becomes `DPT-1.001`.
- Set `readable` and `writable` according to the object's actual role:
  commands are usually writable-only, feedback is readable-only, and bidirectional
  setpoints/status values may be both.
- Set `unit` only when the Datapoint Type has a clear fixed unit. Use `null` for
  dimensionless flags, enums, strings, control fields, and unspecified values.
- Make names and descriptions concise and application-oriented.
- Do not use dummy placeholders such as "Test1", "Example 1", or "Miscellaneous".

## Coverage requirements

1. Parse the complete Datapoint Type table in `KNX-Datapoint-Types.md`.
2. Inventory every existing `datapoint_type` currently present in `comObjects`.
3. Create or retain one practical object for every listed Datapoint Type.
4. A practical object may cover a subtype or group of closely related semantics. It must
   use that exact Datapoint Type and have a credible role in a KNX building application.
5. If a Datapoint Type is already represented by an existing object, you may keep that
   existing object if its name, description, permissions, unit, and role are credible.
   Improve that entry only when needed for practical coverage.
6. If several real devices share the same DPT, choose the most useful and least
   redundant representative object. Do not create duplicate objects merely to equal the
   number of table rows.
7. Ensure the final configuration contains at least one object for each of the 352
   source table entries.
8. Do not add an object for a Datapoint Type that is absent from the table unless the
   firmware or existing tests require such a compatibility object. Every added object
   must be grounded in an identifier found in the reference.
9. Preserve practical command/status conventions where applicable:
   - binary command: writable-only
   - binary feedback: readable-only
   - analog setpoint: readable and writable
   - sensor measurement: readable-only
   - scene or enum control: readable and writable only when feedback exists conceptually

## Selection quality

Prefer objects useful for a complete building demonstration:

- lighting on/off, dimming, brightness, color, and color temperature
- blinds, curtains, windows, and sun protection
- room and duct temperature
- relative humidity, CO2, VOC, pressure, air quality, and illuminance
- heating, cooling, ventilation, HVAC modes, and fan stages
- occupied/vacant, alarm, fault, leak, smoke, and security status
- energy, power, current, voltage, tariff, metering, gas, water, and active/reactive energy
- time, date, scene, clock, calendar, and schedule values
- device diagnostics, firmware/status enums, counters, and maintenance information
- KNX 1-bit variants, 2-/4-/8-/16-/32-/64-bit values, floats, combined values, and strings
  only when their role and encoding are supported by the implementation

Avoid contrived "test" objects. Avoid entries with ambiguous or invalid interpretation
when a more standard building role exists.

## Implementation constraints

- Before editing, inventory every unique Datapoint Type in both the Markdown table and
  the existing JSON.
- Generate missing object entries deterministically.
- Use the next available group-address numbers within consistent three-level application
  areas rather than scattering random addresses.
- Do not introduce address collisions.
- Preserve valid JSON formatting and the existing trailing structure.
- A 352-row source table does not require 352 objects. The required output is coverage:
  one or more practical entries for every listed identifier.
- The firmware's supported/unsupported status must be checked rather than assumed.
  Each final `datapoint_type` must parse or be known to be a valid source-table DPT.
- If the implementation currently rejects a table entry, do not silently add it as a
  valid working object. Record the gap in the final summary and propose the smallest
  required follow-up.

## Verification

At minimum, run the smallest relevant KNX checks:

- Parse and validate `knxConfig.json` as JSON.
- Check the number of source table identifiers.
- Check the number of unique `datapoint_type` values represented by the JSON.
- Check that every source identifier is covered.
- Check that every JSON DPT is found in the reference table.
- Check uniqueness of object IDs.
- Check uniqueness of independent group addresses.
- Check valid three-level addresses.
- Run `node` or the repository's existing KNX test/validation command where practical.
- Run the existing KNX test suite with the project-defined runner or CMake target.

Do not claim coverage beyond what the parser and codec tests prove.

## Required deliverables

1. Modify `tests/knx/knxConfig.json`.
2. Do not rewrite the source DPT reference.
3. Report a concise summary containing:
   - source DPT count
   - existing DPT count
   - number of DPT types added
   - final covered DPT count
   - number of new `comObjects`
   - number of existing objects retained
   - unsupported or unvalidated DPTs
   - checks and test commands run
   - exact final file status

## Acceptance criteria

- The JSON remains syntactically valid.
- Every table identifier is represented in the final configuration.
- Object IDs are unique.
- Independent object group addresses do not collide.
- Every object follows the documented `comObjects` shape.
- The configuration uses practical names and descriptions.
- Existing entries are preserved unless a specific correction is necessary.
- The change is explained and validated, with unsupported codec cases called out rather
  than hidden.
