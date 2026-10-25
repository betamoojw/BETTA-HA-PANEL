#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned count,size_t size);
int xQueueSend(QueueHandle_t,const void*,unsigned);
int xQueueReceive(QueueHandle_t,void*,unsigned);
void vQueueDelete(QueueHandle_t);
