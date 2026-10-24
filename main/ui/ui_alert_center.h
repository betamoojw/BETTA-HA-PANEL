/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "lvgl.h"
#include "util/coredump_store.h"
/* All calls belong to the UI task/display-lock holder. No network or storage I/O. */
enum { UI_ALERT_CRASH_0, UI_ALERT_CRASH_1, UI_ALERT_WIFI, UI_ALERT_HA, UI_ALERT_MQTT,
       UI_ALERT_ENTITY_0, UI_ALERT_SLOTS = UI_ALERT_ENTITY_0 + 4 };
void ui_alert_center_set(unsigned slot, const char *title, const char *detail, uint64_t expires_ms);
void ui_alert_center_clear(unsigned slot);
void ui_alert_center_crash(unsigned index, const coredump_record_t *record);
unsigned ui_alert_center_count(void);
void ui_alert_center_refresh(uint64_t now_ms);
void ui_alert_center_open(void);
void ui_alert_center_close(void);
lv_obj_t *ui_alert_center_root(void);
