/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include "cJSON.h"
/* Pure JSON-RPC dispatcher. NULL means a notification; caller owns response. */
cJSON* knx_mcp_handle(const cJSON* request);
/* Dispatch only MCP envelopes, including during a speech-queue purge. */
cJSON* knx_mcp_message(const cJSON* message, const char* session);
