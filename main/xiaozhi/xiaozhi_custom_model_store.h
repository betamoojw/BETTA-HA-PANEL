/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Custom wake model availability and sequential upload interface.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/** @brief Check the stored model after mounting LittleFS and before HTTP/UI start.
 * Only the pinned, verified resource is exposed to the model parser. No user file is replaced.
 */
void xiaozhi_custom_model_store_init(void);
bool xiaozhi_custom_model_available(void);
/** @return Borrowed versioned storage path; check availability before opening it. */
const char *xiaozhi_custom_model_path(void);

/** @return Sequential bytes accepted by the current upload, not a persisted resume position. */
size_t xiaozhi_custom_model_received(void);

/** @return Existing temporary-file size for storage accounting, or zero if unusable. */
size_t xiaozhi_custom_model_pending_bytes(void);
/** @brief Append a bounded sequential upload chunk from the HTTP task only.
 * Offset zero abandons only this module's incomplete temporary resource.
 *
 * @param offset Byte position expected by the single active upload.
 * @param data Borrowed bytes, not retained after return.
 * @param len Chunk length in bytes, at most 32768.
 * @return True after accepting the chunk. The final chunk also verifies and publishes the model.
 */
bool xiaozhi_custom_model_append(size_t offset, const void *data, size_t len);

#ifdef XIAOZHI_CUSTOM_MODEL_TEST
void xiaozhi_custom_model_test_paths(const char *path, size_t size, const char *sha256);
#endif
