/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Shared evcc loadpoint slots, site power inputs and discovery keys.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "app_config.h"
#include "cJSON.h"

enum
{
    EVCC_MODE,
    EVCC_VEHICLE,
    EVCC_TARGET,
    EVCC_POWER,
    EVCC_SOC,
    EVCC_CONNECTED,
    EVCC_CHARGING,
    EVCC_DISABLED,
    EVCC_EFFECTIVE,
    EVCC_ENERGY,
    EVCC_REMAINING,
    EVCC_ROLE_COUNT
};

/** @brief Maps a layout key and entity domain to an integration registry role. */
typedef struct
{
    const char *key;
    const char *role;
    const char *domain;
} evcc_binding_t;

/** @brief Loadpoint bindings in their runtime slot order. */
extern const evcc_binding_t k_evcc_bindings[EVCC_ROLE_COUNT];

/* The optional header above the controls (evcc_header): the charging power as a bar up to evcc_power_max_kw, or the
 * site's energy flow from these four power sensors (evcc signs: battery > 0 discharges, grid > 0 imports). */
enum
{
    EVCC_SITE_PV,
    EVCC_SITE_BATTERY,
    EVCC_SITE_GRID,
    EVCC_SITE_HOME,
    EVCC_SITE_COUNT
};

/** @brief Site-level power sensor bindings, independent of the selected loadpoint. */
extern const evcc_binding_t k_evcc_site_bindings[EVCC_SITE_COUNT];
#define EVCC_POWER_MAX_KW_DEFAULT 11.0f
#define EVCC_POWER_MAX_KW_MIN     1.0f
#define EVCC_POWER_MAX_KW_MAX     50.0f
/** @return True for an accepted loadpoint domain, including the legacy select target. */
bool evcc_binding_domain(size_t role, const char *id);
/** @brief Copy saved loadpoint IDs into the three primary slots and the bounded extra array.
 *
 * Each supplied destination holds APP_MAX_ENTITY_ID_LEN bytes. Missing or
 * oversized values leave the prior slot contents unchanged.
 */
void evcc_slots_from_json(const cJSON *widget, char *primary, char *secondary, char *tertiary, char (*extra)[APP_MAX_ENTITY_ID_LEN],
                          size_t extra_count);
/** @brief Decode the optional power or energy-flow header into caller-owned storage.
 * @param widget Borrowed widget JSON.
 * @param[out] mode Optional string buffer for "power", "flow", or empty when disabled.
 * @param mode_len Capacity of mode in bytes, including the NUL.
 * @param[out] max_kw Optional scale in kW; missing or invalid values use the default.
 * @param[out] site Optional array of site sensor destinations, each APP_MAX_ENTITY_ID_LEN bytes.
 */
void evcc_header_from_json(const cJSON *widget, char *mode, size_t mode_len, float *max_kw, char *const site[EVCC_SITE_COUNT]);
/** @brief Classify a loadpoint by integration registry identity, not by entity ID suffix.
 * @return True on a domain and role match; writes the full instance/loadpoint stem.
 */
bool evcc_registry_role(const char *id, const char *unique_id, char *stem, size_t capacity, size_t *role);
/** @brief Match an instance-level power sensor whose registry stem has no loadpoint component. */
bool evcc_registry_site_role(const char *id, const char *unique_id, size_t *role);
