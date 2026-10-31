/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
/* Worker-owned PSRAM buffer. Retain until srmodel is destroyed. */
void *xiaozhi_wake_model_load(int word);
void xiaozhi_wake_model_free(void *data);
