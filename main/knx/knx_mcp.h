/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include "cJSON.h"
/* Xiaozhi JSON-RPC dispatcher. Common tools precede the KNX board tools and
 * user-only tools are listed only after an explicit withUserTools request.
 * NULL means a notification; caller owns the response. */
cJSON* knx_mcp_handle(const cJSON* request);
/* Dispatch only MCP envelopes, including during a speech-queue purge. */
cJSON* knx_mcp_message(const cJSON* message, const char* session);
/* Drop per-connection User Only opt-in on disconnect. */
void knx_mcp_session_reset(void);
