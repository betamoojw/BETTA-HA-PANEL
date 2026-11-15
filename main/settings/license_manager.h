/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#define LICENSE_TRIAL_SECONDS 3600U
typedef enum { LICENSE_MISSING, LICENSE_VALID, LICENSE_INVALID, LICENSE_ERROR, LICENSE_UNCONFIGURED } license_key_state_t;
typedef struct {
    char device_id[65];
    license_key_state_t key_state;
    uint32_t trial_seconds;
} license_status_t;
/* Initialize once after NVS, before starting tasks. Import/remove belong to HTTP task. */
void license_manager_init(void);
void license_manager_status(license_status_t *out);
esp_err_t license_manager_import(const void *key, size_t length);
esp_err_t license_manager_remove(void);
/* Status only: no API in this module enables/disables any panel functionality. */
#ifdef __cplusplus
}
#endif
