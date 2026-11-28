/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Farewell recognition for user speech transcripts.
 */
#pragma once
#include <stdbool.h>
/** @brief Check a confirmed user speech transcript; never pass assistant text.
 * @return True for an entire supported short farewell, not a substring or quoted example.
 */
bool xiaozhi_farewell(const char *text);
