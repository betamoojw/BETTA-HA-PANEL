/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */

/** @file
 * @brief Xiaozhi MCP request and connection lifecycle API.
 */
#pragma once
#include "cJSON.h"

/**
 * @brief Dispatch JSON-RPC requests. User-only tools require explicit withUserTools authorization.
 * @return Response owned by the caller, or NULL for a notification with no request ID.
 */
cJSON *knx_mcp_handle(const cJSON *request);

/**
 * @brief Dispatch only MCP envelopes, including during a speech-queue purge.
 * @return Response envelope owned by the caller, or NULL if no response is produced.
 */
cJSON *knx_mcp_message(const cJSON *message, const char *session);

/** @brief Clear the current connection's user-only tool authorization. */
void knx_mcp_session_reset(void);
