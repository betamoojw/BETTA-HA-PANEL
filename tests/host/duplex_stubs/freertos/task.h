/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "FreeRTOS.h"
typedef struct mock_task *TaskHandle_t;
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
TickType_t xTaskGetTickCount(void);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
void vTaskDelete(TaskHandle_t);
