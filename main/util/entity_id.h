/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared Home Assistant entity ID validation and notification action allowlist.
 */
#pragma once

#include <stdbool.h>

/** @return True for a bounded `domain.object_id` with lowercase letters, digits, or underscores on either side. */
bool entity_id_valid(const char *entity_id);
/** @return True for a valid ID whose complete domain matches the supplied domain. */
bool entity_id_in_domain(const char *entity_id, const char *domain);
/** @return True for a valid ID in a domain supported by notification or alert actions. */
bool entity_id_action_allowed(const char *entity_id);
