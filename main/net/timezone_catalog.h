/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
/** @file
 * @brief Read-only lookup in the compiled timezone and date-pattern catalog.
 *
 * Returned strings have static lifetime. A name is not interchangeable with
 * its rules because several locations can share the same recurring rules.
 */
#pragma once
#include <stddef.h>
/** @return Catalog location at this zero-based index, or NULL out of range. */
const char *timezone_catalog_name(size_t index);
/** @return Recurring POSIX timezone rules for an exact name, or NULL. */
const char *timezone_catalog_rules(const char *name);
/** @return Numeric display date pattern for the location, or ISO order if unknown. */
const char *timezone_catalog_date_pattern(const char *name);
