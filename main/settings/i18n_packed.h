/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include "cJSON.h"

/* No shared scratch state. The caller owns the returned JSON tree. */
cJSON *i18n_packed_parse(const unsigned char *data, size_t size, size_t json_size);
