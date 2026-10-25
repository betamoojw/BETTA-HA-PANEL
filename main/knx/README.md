# KNX implementation

Shared KNXnet/IP Routing service and WebUI/MCP boundary.
See [development guide](../../docs/development/knx.md) and [user guide](../../docs/knx.md).

Object codecs, subtype tables, configuration parser and JSON value conversion
are adapted from betamoojw/xiaozhi-esp32 feat/knx_ip,
revision 61fc9d0b619f39ed20356b8de2f9c0edd954472d.
Its MIT notice is retained in LICENSE.xiaozhi.
The service, storage, API and MCP dispatch are BETTA-specific.

Codec tests retain that provenance. The external esp_knx_ip transport/DPT
dependency has separate terms; see the development guide.
