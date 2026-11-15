/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdlib.h>
typedef int *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateCounting(unsigned max, unsigned initial) { (void)max; int *p = malloc(sizeof(int)); if (p) *p = initial; return p; }
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return xSemaphoreCreateCounting(1, 1); }
static inline int xSemaphoreTake(SemaphoreHandle_t p, unsigned wait) { (void)wait; if (*p <= 0) return 0; --*p; return 1; }
static inline void xSemaphoreGive(SemaphoreHandle_t p) { ++*p; }
