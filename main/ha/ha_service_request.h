/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdint.h>
#include "cJSON.h"

/* Complete WS service request or NULL. A supplied payload must parse fully as
 * an object; malformed data/OOM must never turn a targeted call into a global one. */
cJSON *ha_service_request_create(uint32_t id, const char *domain, const char *service, const char *json_data);
