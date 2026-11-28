/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Content checks for runtime settings strings that the length limits in api_settings.c do not cover.
 * Pure functions, host-tested. An empty string is accepted by every check: it means "use the default" or "unset".
 */
/** @file
 * @brief Pure input checks for persisted connection targets and display text.
 *
 * Empty strings are accepted; a NULL pointer is not.
 */
#pragma once

#include <stdbool.h>

/** @brief Check the allowed hostname/address characters without resolving the host. */
bool settings_host_valid(const char *host);
/** @brief Require printable ASCII in a timezone string; does not parse the timezone rules. */
bool settings_timezone_valid(const char *tz);
/** @brief Check a topic prefix for printable non-space ASCII, no wildcards and no empty path components. */
bool settings_topic_prefix_valid(const char *prefix);
/** @brief Reject ASCII control bytes in display text without restricting non-ASCII text. */
bool settings_text_valid(const char *text);
