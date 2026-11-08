/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
bool alarm_panel_validate(const cJSON *widget, char *error, size_t size);
