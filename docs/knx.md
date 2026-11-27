<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# KNX

KNXnet/IP connects configured communication objects over the panel's existing
Wi-Fi connection. Open **Settings → KNXnet/IP** in the web editor. It is disabled
by default and does not require Home Assistant to relay KNX telegrams.

Use a routing-capable KNX/IP router on the same multicast-capable LAN.
This implementation supports Routing, not Tunneling, discovery or KNX Secure.
The panel and router must share the configured multicast address and UDP port.
Choose an individual address that is unused in your KNX installation.

## Configuration

1. Import **knxConfig.json**, or edit the JSON in the configuration field.
2. Check the physical address, multicast endpoint and communication objects.
3. Select **Save configuration**, then enable KNXnet/IP.
4. Request a read of a known readable object.
5. After bus feedback, select **Refresh cached values**.

Imports remain drafts until saved. **Export draft** includes unsaved edits.
**Reload saved** asks before discarding edits. Disabling retains the saved
configuration. Saving replaces the registry atomically and restarts Routing
without rebooting. Invalid configuration leaves the previous file and objects intact.
If a stored file is damaged, its text remains editable and exportable for recovery.

The panel backup includes the KNX configuration and enable preference. If a
backup contains more than 512 communication objects, the editor shows the limit
in your current language before writing any part of the backup. If the panel rejects the
registry for another reason, restoration also stops before changing other settings.
Older backups that omit KNX preserve the panel's existing registry.

~~~json
{
  "media_type": "knx_ip",
  "media_parameters": {
    "multicast_address": "224.0.23.12",
    "udp_port": 3671,
    "transport_mode": "routing",
    "interface_identifier": "KNX-IP-Interface",
    "nat": false
  },
  "physical_address": "15.15.199",
  "comObjects": [
    {
      "id": "living_light",
      "name": "Living room light",
      "description": "Switch command for the ceiling light",
      "group_address": "1/0/1",
      "datapoint_type": "DPT-1.001",
      "readable": false,
      "writable": true,
      "unit": null
    },
    {
      "id": "living_temperature",
      "name": "Living room temperature",
      "group_address": "1/0/2",
      "datapoint_type": "DPT-9.001",
      "readable": true,
      "writable": false,
      "unit": "°C"
    }
  ]
}
~~~

Use your installation's addresses and datapoints; this example is synthetic.
Each object needs a unique ID. Objects sharing a group address must use the same DPT. Use separate command and feedback objects
when actuators have separate group addresses. A feedback object must be
readable and may also be writable. The nat field must be false for Routing.
The interface_identifier is a descriptive label; the panel uses its existing Wi-Fi interface.

One registry can hold up to **512 communication objects**, including 512 distinct
group addresses. The JSON file can contain up to **256 KiB (262,144 UTF-8 bytes)**,
including whitespace. IDs allow 48 bytes, names 80,
descriptions 192 and units 32. Group addresses use main/middle/sub with ranges
0–31, 0–7 and 0–255. Individual addresses use area.line.member with ranges
0–15, 0–15 and 0–255. At least one permission must be true.
Unknown/duplicate fields and unsupported DPTs reject the entire import.

## Dashboard widgets

In **Layout → + Widget**, select **KNX** as the data source, then choose the widget type.
Assign objects by purpose, select **Use bindings**, then **Save** the layout. Each selector
shows the object's name, ID, group address, DPT and access (R, W or R/W). If a selector
has no candidates, its hint identifies the required access and DPT.

| Widget | Command (writable) | Feedback (readable) |
| --- | --- | --- |
| Button in switch mode | Switch command, DPT 1 (for example 1.001) | Switch feedback, DPT 1 |
| Slider | Absolute percentage, DPT 5.001 (0–100%), or angle, DPT 5.003 (0–360°) | Optional readable object of the same subtype |
| Select | HVAC operating preset, DPT 20.102 | Optional operating preset, DPT 20.102 |
| Light | Switch command and brightness command | Switch feedback and brightness feedback, independently. Switching uses DPT 1 and brightness uses DPT 5.001. |
| Sensor | — | Any supported DPT, including an R/W object |
| Gauge, Graph | — | Numeric DPT 5–9 (except 6.020), 12–14 or 29. Graph keeps local samples. |
| Binary, Contact | — | DPT 1 |
| Scene / fixed-value action | Any supported writable DPT and a fixed value in the indicated format | — |
| Cover | Move, DPT 1.008 or 1.009; optional stop, DPT 1.010 or 3.008; optional absolute position, DPT 5.001 | Actual position, DPT 5.001 |
| Climate | Absolute target temperature, DPT 9.001; optional HVAC control mode, DPT 20.105 | Target temperature and current temperature, separately, DPT 9.001; optional control mode, DPT 20.105 |
| Fan | Switch command, DPT 1; percentage speed command, DPT 5.001 | Switch feedback, DPT 1; actual percentage speed, DPT 5.001 |

For a dimmer, choose **Slider**. Bind **Brightness command** to the actuator's input
(for example `2/0/2`) and **Brightness feedback** to its status address (for example `2/1/2`).
If you request 80% and the actuator reports 62%, the LCD and editor show 62%.
An R/W object can occupy either role or both. **Light** accepts four separate objects.
For an on/off light, assign just the two switch roles. Its brightness bar stays hidden.

A status address is optional for each command with a matching feedback role. Leave it
unassigned to show the last command sent successfully to that object, including Off
and 0%. Other widgets bound to the same command address see the same value. A write
from another KNX participant to that address also updates it. Until a command is known
the value is unknown. You can send an explicit value immediately, and the first
command-only power toggle sends On.

When a feedback address is assigned, only valid external feedback supplies that reading.
A missing status object, local echo or failed send cannot substitute the requested value.
Clear just that feedback selector to display commands again. The cache starts empty
after routing reconnects or the registry is replaced. Sensors and graphs always use readings.

For an angle slider choose the angle roles for both directions. The editor keeps angles
and percentages separate. The angle uses one byte across 0–360°, so actuator feedback
can differ from the requested angle by a rounding step. For **Select**, a writable
DPT 20.102 offers Auto (0), Comfort (1), Standby (2), Economy (3) and Building Protection (4).
Use a readable object of that subtype for status if the actuator provides one.
The panel and editor show **Unavailable** while KNX routing is offline. After reconnection
they show `--` until a new command is sent or, if status is bound, new feedback arrives.
A reserved operating preset reported by the actuator clears the bound reading
to `--`. The next valid report restores it, including Auto (0).
DPT 20.105 HVAC control modes and DPT 22.101 compound status use different encodings.

DPT 5.001 Sensor tiles show whole percentages by default, for example `62%`. An optional
`display_precision` on the communication object selects 0–6 decimal places. The objects API
retains the decoded value. Other sensor types keep their usual formatting.

Use **Edit KNX bindings** in the Inspector to change or clear one role while keeping the others.
A missing object keeps its saved ID. Restoring that ID reconnects the binding, even if the group
address changes. For an existing single-object RGB Light, choose another readable and writable
DPT 232 object in the RGB selector to keep colour control. Choose **Unassigned** there to use
separate switch and brightness roles. Changing a Scene's target clears its fixed value.
Enter a value for the new object, or leave it empty for a text DPT that accepts empty strings.
**Home Assistant** in the Inspector selects an HA entity.

### Cover

Choose **Cover**, then assign **Up/down command** and **Position feedback**. Up/open sends
zero to a DPT 1.008/1.009 move object; down/close sends one. If only **Position command**
is assigned, the arrow buttons send the corresponding endpoint percentage instead.
The existing cover bar shows position; it is not an arbitrary-position slider.

The default is the usual KNX closed percentage: 0% means fully open and 100% fully closed.
The tile shows an open percentage, so a KNX value of 70% appears as 30% open.
Enable **KNX 100% means fully open** only for an actuator configured with the opposite
percentage direction. The option applies to both position commands and feedback, survives
save/reload, and does not reverse the move command's defined polarity.

The stop button is available only with an explicitly bound stop object. DPT 1.010 sends
zero (stop); DPT 3.008 sends the break code. DPT 1.007 is a step command and is not offered
as a universal stop. Check the actuator's ETS application before choosing its objects.
The tile does not infer travel time or movement from the last requested position.

### Climate

Bind **Target temperature command**, **Target temperature feedback** and **Measured
temperature feedback** independently. Only absolute Celsius DPT 9.001 is accepted;
a DPT 9.002 temperature difference is not an absolute setpoint. The existing climate
tile uses 0.5°C adjustments within 7–35°C. Its plus/minus buttons need a writable target
and a known target, supplied by its feedback or by the last command when feedback is
unassigned. The draft is sent after the 800 ms input delay. Measured temperature
always comes from its own readable object.

Optional HVAC control mode objects must use DPT 20.105. The selector offers Auto (0),
Heat (1), Cool (3), Off (6), Fan only (9) and Dry (14). Use these only when supported by
the configured controller; the DPT does not advertise its installed equipment capabilities.
Unmapped incoming mode values show no assumed mode. DPT 20.102 operating presets such as
Comfort and Economy are not substituted for heat/cool control modes. Presets, temperature
offsets, fan modes and swing modes are not added by this KNX climate adapter.

Missing live feedback or the DPT 9 invalid-data marker clears the corresponding reading
without clearing the other roles. Other malformed telegrams leave the last valid cache intact.
Restoring valid bus feedback restores the reading and eligible controls.

### Fan

Switch power and percentage speed use separate command/feedback pairs. A speed report
does not imply that the fan is on. With a status address the power button waits for
switch feedback. Slower/faster needs a known speed. The large tile's four speed buttons send
25%, 50%, 75% or 100% and can be used before speed feedback when the command is writable.
These are percentage presets, not a claim that the actuator has four hardware speeds.

This adapter accepts DPT 5.001 speed percentages. DPT 5.010 step counts require an explicit
step-count mapping and are not accepted here. DPT 20.111 describes fan operating behavior,
not low/medium/high speed. Oscillation and device-specific presets remain HA-only controls.

All three tiles reuse the existing panel styles and support feedback-only bindings.
Missing commands disable the corresponding controls. Routing disconnects disable commands;
each role needs a new command or its bound external feedback after reconnect. Editor previews use the same
KNX cache instead of example HA values. Existing HA bindings retain their behavior.

## Values and voice control

Codecs cover families 1–31, 232, 234 and 251, with the reference adapter's subtype
validation. Not every subtype number is supported. The configured DPT determines
the accepted value; the AI cannot supply another DPT or arbitrary destination.
Object details include a value_format hint.

Common inputs are on/off for DPT-1.001, a percentage for DPT-5.001 and
a decimal number for DPT-9.001. Compound values use comma-separated fields.
Reads and writes are asynchronous. **Routing active** means the multicast socket
is open; it does not prove bus availability or actuator execution.
Cached state becomes valid only after an incoming write/response from another
individual address, never from a local send or multicast loopback.
After reconnect, consider the cached timestamp before using an old value.

On panels supporting [AI Assistant](xiaozhi-ai.md), enable/pair the assistant as usual.
For example, ask it to turn on the configured living room light or read its
temperature. AI Assistant discovers IDs, names and permissions through MCP.
Configuration changes and enabling KNX belong to the editor, not AI tools.
Existing wake-up, dialogue, emoji and timeout behavior remains.
Other profiles provide KNX through the editor/API; this does not add audio
drivers to hardware without assistant audio support.

## Validation

Local protocol, codec, configuration and MCP tests use synthetic objects.
A firmware build is not proof of a real KNX bus connection.
The Waveshare 10.1-inch P4 device trial verified editor import/export, persistence,
enable/disable and WLED power control with real feedback and state restoration.
The owner also verified Xiaozhi voice on/off control, replies and idle auto-close.
Other datapoints and hardware profiles need their own validation.
The previously reported [intermittent white flashes](development/knx.md#unresolved-white-flashes)
remain unresolved; successful KNX testing does not establish a display fix.
See [KNX development](development/knx.md) for API, tests and provenance.
