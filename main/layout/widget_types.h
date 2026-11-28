/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The widget types in one table: which types exist, their size limits on this variant and which entity
 * domains their primary entity may come from. The layout validator and the runtime (clamping) both read it,
 * so a new type is one row here instead of a branch in each of them.
 */
/** @file
 * @brief Widget type registry, display-name policy and size constraints.
 */
#pragma once

#include <stdbool.h>
#include "cJSON.h"

/** @brief Inclusive width and height bounds in logical pixels. */
typedef struct
{
    int min_w;
    int min_h;
    int max_w;
    int max_h;
} widget_size_limits_t;

/** @brief Smallest LED block count accepted for a segmented linear gauge. */
#define GAUGE_SEGMENT_COUNT_MIN 4
/** @brief Largest LED block count accepted for a segmented linear gauge. */
#define GAUGE_SEGMENT_COUNT_MAX 30
/** @brief Default segmented gauge count when the layout has no override. */
#define GAUGE_SEGMENT_COUNT_DEFAULT 10

/** @return True for a known type, including the legacy roborock_tile alias. */
bool widget_type_known(const char *type);
/** @brief Normalize retired widget formats in a caller-owned tree without writing storage.
 * @return False on allocation failure; discard the possibly partially normalized tree.
 */
bool widget_types_normalize_layout(cJSON *layout);
/** @return True when the type is known and supports a separate display name. */
bool widget_type_supports_name(const char *type);
/** @return Explicit name visibility, then the legacy sensor setting if present.
 * Defaults to hidden for weather tiles and visible otherwise.
 */
bool widget_show_name(const cJSON *widget);
/** @return Board-clamped size bounds for a type, or generic bounds for an unknown type. */
widget_size_limits_t widget_type_size_limits(const char *type);
/** @return Look-specific bounds when defined; otherwise the type's default bounds. */
widget_size_limits_t widget_size_limits_for(const char *type, const char *style_variant);
/** @return True for an accepted primary entity domain. Unknown types are checked separately by the validator. */
bool widget_type_entity_domain_ok(const char *type, const char *entity_id);
/** @brief Write accepted domain patterns for a diagnostic, using ?.* for an unknown type. */
void widget_type_domain_hint(const char *type, char *out, unsigned out_len);
