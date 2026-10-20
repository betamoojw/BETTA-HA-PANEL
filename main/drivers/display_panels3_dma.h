/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
void display_panels3_dma_begin(void);
void display_panels3_dma_end(void);

/* Storage-completion notification; no LVGL or LCD access from a writer. */
void display_panels3_flash_complete(void);
