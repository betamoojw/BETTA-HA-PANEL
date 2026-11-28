/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Layout limit metadata shared by the layout and version endpoints.
 */
#pragma once
#include "cJSON.h"

/** @brief Append current-orientation geometry and layout budgets to a caller-owned JSON object.
 *
 * Report the current page limit separately from the total storage capacity.
 */
void api_layout_add_limits(cJSON *root);
