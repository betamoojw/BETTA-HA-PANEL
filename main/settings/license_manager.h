/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Informational license state and key persistence.
 *
 * License status does not enable or disable panel functionality.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define LICENSE_TRIAL_SECONDS 3600U /**< Informational trial length per boot, in seconds. */

/** @brief Distinguish a missing or invalid record from operational and build errors. */
typedef enum
{
    LICENSE_MISSING,
    LICENSE_VALID,
    LICENSE_INVALID,
    LICENSE_ERROR,
    LICENSE_UNCONFIGURED
} license_key_state_t;

/** @brief Read-only metadata snapshot; does not include license bytes. */
typedef struct
{
    char device_id[65];
    license_key_state_t key_state;
    uint32_t trial_seconds;
} license_status_t;

/** @brief Load the key state and derive the device identity once after NVS and before tasks start. */
void license_manager_init(void);
/** @brief Copy status without reading storage. A valid key reports zero remaining trial seconds. */
void license_manager_status(license_status_t *out);
/** @brief Verify and commit a device-bound key from the HTTP task before reporting it valid. */
esp_err_t license_manager_import(const void *key, size_t length);
/** @brief Delete the stored license from the HTTP task; absence is accepted. */
esp_err_t license_manager_remove(void);
#ifdef __cplusplus
}
#endif
