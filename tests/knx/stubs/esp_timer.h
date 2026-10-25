#pragma once
#include <stdint.h>
extern int64_t test_now;
inline int64_t esp_timer_get_time() { return test_now; }
