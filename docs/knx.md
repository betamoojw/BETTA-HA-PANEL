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

The panel backup includes KNX configuration and enable preference.
Older backups without KNX leave its settings unchanged. Existing local backup files
are never rewritten by this feature.

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
Each ID and group address must be unique. Use separate command and feedback objects
when actuators have separate group addresses. Sensor/status objects should be
readable only. The nat field must be false for Routing.
The interface_identifier is a descriptive label; the panel uses its existing Wi-Fi interface.

Limits are 128 objects and 65,535 UTF-8 bytes. IDs allow 48 bytes, names 80,
descriptions 192 and units 32. Group addresses use main/middle/sub with ranges
0–31, 0–7 and 0–255. Individual addresses use area.line.member with ranges
0–15, 0–15 and 0–255. At least one permission must be true.
Unknown/duplicate fields and unsupported DPTs reject the entire import.

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

On panels supporting [Xiaozhi AI](xiaozhi-ai.md), enable/pair the assistant as usual.
For example, ask it to turn on the configured living room light or read its
temperature. Xiaozhi discovers IDs, names and permissions through MCP.
Configuration changes and enabling KNX belong to the editor, not AI tools.
Existing wake-up, dialogue, emoji and timeout behavior remains.
Other profiles provide KNX through the editor/API; this does not add audio
drivers to hardware without Xiaozhi support.

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
