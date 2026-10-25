/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Called once after mounting LittleFS, before HTTP/UI start. Only a pinned,
 * verified resource is exposed to the font parser. No user file is replaced. */
void xiaozhi_font_store_init(void);
bool xiaozhi_font_available(void);
const char *xiaozhi_font_path(void);
size_t xiaozhi_font_received(void);
size_t xiaozhi_font_pending_bytes(void);
/* HTTP task is the single upload owner. Chunks are bounded and sequential;
 * offset zero abandons only our incomplete temporary resource. */
bool xiaozhi_font_append(size_t offset, const void *data, size_t len);

#ifdef XIAOZHI_FONT_TEST
void xiaozhi_font_test_paths(const char *path, size_t size, const char *sha256);
#endif
