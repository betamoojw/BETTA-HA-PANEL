#pragma once
#include "FreeRTOS.h"
extern void (*test_worker)(void*);
inline int xTaskCreate(void (*fn)(void*),const char*,unsigned,void*,int,TaskHandle_t* task) {
    test_worker=fn; *task=(void*)1; return pdPASS;
}
