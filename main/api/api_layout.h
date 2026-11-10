/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "cJSON.h"

/* The same current-orientation budget in /api/version and /api/layout?limits=1. */
void api_layout_add_limits(cJSON *root);
