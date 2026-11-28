/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Built-in wake model buffer lifetime.
 */
#pragma once
/** @return Worker-owned PSRAM buffer to retain until the speech model is destroyed, or NULL on failure. */
void *xiaozhi_wake_model_load(int word);

/** @brief Free the backing buffer only after the speech model has released it. */
void xiaozhi_wake_model_free(void *data);
