/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Called once after mounting LittleFS, before HTTP/UI start. Only a pinned,
 * verified resource is exposed to the model parser. No user file is replaced. */
void xiaozhi_custom_model_store_init(void);
bool xiaozhi_custom_model_available(void);
const char *xiaozhi_custom_model_path(void);
size_t xiaozhi_custom_model_received(void);
size_t xiaozhi_custom_model_pending_bytes(void);
/* HTTP task is the single upload owner. Chunks are bounded and sequential;
 * offset zero abandons only our incomplete temporary resource. */
bool xiaozhi_custom_model_append(size_t offset, const void *data, size_t len);

#ifdef XIAOZHI_CUSTOM_MODEL_TEST
void xiaozhi_custom_model_test_paths(const char *path, size_t size, const char *sha256);
#endif
