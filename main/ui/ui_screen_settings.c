/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Native settings inspired by Cpt_Kirk's ui_screen_settings in the Guition fork.
 * Shared settings policy, semantic typography and deferred storage are used here.
 */
#include "ui/ui_transition.h"
#include "ui/ui_screen_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/timezone_catalog.h"
#include "ui/theme/theme_palette.h"
#include "app_config.h"
#include "settings/screen_settings_service.h"
#include "ui/fonts/app_symbols.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/theme_default.h"
#include "ui/theme/ui_typography.h"
#include "ui/ui_i18n.h"
#include "ui/ui_memory.h"
#include "ui/ui_display_protection.h"

typedef struct {
    screen_settings_t draft;
    screen_settings_t original;
    lv_obj_t *root, *body, *message, *save, *cancel;
    lv_obj_t *rows[SCREEN_FIELD_COUNT], *controls[SCREEN_FIELD_COUNT], *values[SCREEN_FIELD_COUNT];
    lv_timer_t *timer;
    bool saving;
    bool closing;
    bool preview;
    lv_obj_t *system_controls[SYSTEM_FIELD_COUNT];
    int choices[SCREEN_FIELD_COUNT][10];
} settings_view_t;
static settings_view_t *s_view;

static const char *const keys[] = {"active", "dim", "dim_seconds", "mode", "start_minutes",
    "clock_start", "clock_brightness", "clock_off", "clock_off_seconds", "clock_24h", "clock_seconds", "clock_date", "clock_analog", "rotation", "auto_rotation", "xiaozhi_emoji"};
static const char *const names[] = {"Brightness", "Dimmed brightness", "Dim after (seconds)", "After inactivity",
    "Start after (minutes)", "Start clock after (seconds)", "Clock brightness", "Turn off after clock",
    "Show clock for (seconds)", "24-hour clock", "Show seconds", "Show date", "Analog clock", "Display rotation", "Automatic rotation", "AI emotion style"};

static const char *tr(const char *key, const char *fallback)
{
    char path[64];
    snprintf(path, sizeof(path), "screen.%s", key);
    return ui_i18n_get(path, fallback);
}

/* First option always preserves the saved value, including custom translations,
 * imported palettes and legacy POSIX zones. Other options share web registries. */
static const char *system_id(settings_view_t *view, int field, unsigned index)
{
    static const char *languages[] = {"en", "de", "es", "fr", "pl", "zh-cn", "zh-tw"};
    if (field == SYSTEM_ANIMATIONS) {
        if (index == 0) return view->original.page_animations ? "on" : "off";
        return index == 1 ? "on" : index == 2 ? "off" : NULL;
    }
    if (index == 0) {
        return field == SYSTEM_LANGUAGE ? view->original.language : field == SYSTEM_TIMEZONE ? view->original.timezone :
            field == SYSTEM_STYLE ? view->original.style : view->original.theme;
    }
    index--;
    if (field == SYSTEM_LANGUAGE) return index < 7 ? languages[index] : NULL;
    if (field == SYSTEM_TIMEZONE) return timezone_catalog_name(index);
    if (field == SYSTEM_STYLE) {
        ui_style_kind_t kind;
        return ui_style_at(index, &kind) ? ui_style_name(kind) : NULL;
    }
    const theme_entry_t *themes[16];
    size_t count = theme_palette_builtin_list(themes);
    if (index < count) return themes[index]->id;
    return index - count < view->draft.custom_theme_count ? view->draft.custom_theme_ids[index - count] : NULL;
}

static const char *system_name(settings_view_t *view, int field, const char *id)
{
    static const char *codes[] = {"en", "de", "es", "fr", "pl", "zh-cn", "zh-tw"};
    static const char *names[] = {"English", "Deutsch", "Español", "Français", "Polski", "简体中文", "繁體中文"};
    if (field == SYSTEM_LANGUAGE) for (int i = 0; i < 7; i++) if (!strcmp(id, codes[i])) return names[i];
    if (field == SYSTEM_THEME) for (unsigned i = 0; i < view->draft.custom_theme_count; i++)
        if (!strcmp(id, view->draft.custom_theme_ids[i])) return view->draft.custom_theme_names[i];
    if (field == SYSTEM_STYLE || field == SYSTEM_THEME || field == SYSTEM_ANIMATIONS) {
        char key[64]; snprintf(key, sizeof(key), "system.%s", id);
        const theme_entry_t *theme = field == SYSTEM_THEME ? theme_palette_find_builtin(id) : NULL;
        return ui_i18n_get(key, theme != NULL ? theme->name : id);
    }
    return id;
}

static void system_changed(lv_event_t *event)
{
    if (s_view == NULL || s_view->saving || s_view->closing) return;
    int field = (int)(uintptr_t)lv_event_get_user_data(event);
    unsigned index = lv_dropdown_get_selected(lv_event_get_target_obj(event));
    /* Keep index zero's label and value immutable until the view closes. */
    const char *id = system_id(s_view, field, index);
    if (id == NULL) return;
    s_view->draft.dirty |= SYSTEM_DIRTY(field);
}

static void system_collect(settings_view_t *view)
{
    for (int field = 0; field < SYSTEM_FIELD_COUNT; field++) {
        if (!(view->draft.dirty & SYSTEM_DIRTY(field))) continue;
        unsigned selected = lv_dropdown_get_selected(view->system_controls[field]);
        const char *id = system_id(view, field, selected);
        if (field == SYSTEM_ANIMATIONS) view->draft.page_animations = strcmp(id, "on") == 0;
        if (field == SYSTEM_LANGUAGE) snprintf(view->draft.language, sizeof(view->draft.language), "%s", id);
        if (field == SYSTEM_TIMEZONE) snprintf(view->draft.timezone, sizeof(view->draft.timezone), "%s", id);
        if (field == SYSTEM_STYLE) snprintf(view->draft.style, sizeof(view->draft.style), "%s", id);
        if (field == SYSTEM_THEME) snprintf(view->draft.theme, sizeof(view->draft.theme), "%s", id);
    }
}

static lv_obj_t *container(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(object, flow);
    lv_obj_set_style_pad_row(object, 10, 0);
    lv_obj_set_style_pad_column(object, 12, 0);
    lv_obj_set_scrollable(object, false);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, ui_text_role_t role)
{
    lv_obj_t *object = lv_label_create(parent);
    ui_text_set(object, role, false);
    lv_obj_set_style_text_color(object, lv_color_hex(APP_UI_COLOR_TEXT_PRIMARY), 0);
    lv_label_set_text(object, text);
    return object;
}

static void refresh(settings_view_t *view)
{
    int mode = view->draft.value[SCREEN_MODE];
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) {
        bool hidden = (i >= SCREEN_CLOCK_START && i <= SCREEN_CLOCK_ANALOG && i != SCREEN_CLOCK_24H && mode != 1) ||
            (i == SCREEN_START_MINUTES && (mode == 0 || mode == 1)) ||
            (i == SCREEN_CLOCK_OFF_SECONDS && !view->draft.value[SCREEN_CLOCK_OFF]);
        if (i == SCREEN_ROTATION && view->draft.value[SCREEN_AUTO_ROTATION]) hidden = true;
        if (i == SCREEN_XIAOZHI_EMOJI && !view->draft.xiaozhi_supported) hidden = true;
#if !defined(CONFIG_APP_PANEL_VARIANT_TAB5_1280)
        if (i == SCREEN_AUTO_ROTATION) hidden = true;
#endif
        lv_obj_set_hidden(view->rows[i], hidden);
        if (view->values[i] != NULL) {
            char value[24];
            snprintf(value, sizeof(value), "%d%s", view->draft.value[i],
                i == SCREEN_ACTIVE || i == SCREEN_DIM || i == SCREEN_CLOCK_BRIGHTNESS ? "%" : "");
            lv_label_set_text(view->values[i], value);
        }
    }
}

static void changed(lv_event_t *event)
{
    settings_view_t *view = s_view;
    if (view == NULL || view->saving || view->closing) return;
    int field = (int)(uintptr_t)lv_event_get_user_data(event);
    lv_obj_t *control = lv_event_get_target_obj(event);
    bool timeout = field == SCREEN_DIM_SECONDS || field == SCREEN_START_MINUTES ||
        field == SCREEN_CLOCK_START || field == SCREEN_CLOCK_OFF_SECONDS;
    int value = timeout ? view->choices[field][lv_dropdown_get_selected(control)] :
        (field == SCREEN_MODE || field == SCREEN_ROTATION || field == SCREEN_XIAOZHI_EMOJI) ? (int)lv_dropdown_get_selected(control) :
        (field == SCREEN_CLOCK_OFF || field >= SCREEN_CLOCK_24H ? lv_obj_has_state(control, LV_STATE_CHECKED) :
        lv_slider_get_value(control));
    view->draft.value[field] = value;
    view->draft.dirty |= 1U << field;
    /* Show the effective dimmed level, rather than saving a surprising normalization. */
    if ((field == SCREEN_ACTIVE || field == SCREEN_DIM) && view->draft.value[SCREEN_DIM] > view->draft.value[SCREEN_ACTIVE]) {
        view->draft.value[SCREEN_DIM] = view->draft.value[SCREEN_ACTIVE];
        view->draft.dirty |= 1U << SCREEN_DIM;
        lv_slider_set_value(view->controls[SCREEN_DIM], view->draft.value[SCREEN_DIM], LV_ANIM_OFF);
    }
    refresh(view);
}

static void deleted(lv_event_t *event)
{
    settings_view_t *view = lv_event_get_user_data(event);
    if (view->timer != NULL) lv_timer_delete(view->timer);
    if (view->preview) ui_display_brightness_test_stop();
    if (s_view == view) s_view = NULL;
    free(view);
}

static void close_view(settings_view_t *view)
{
    if (view->closing) return;
    view->closing = true;
    if (view->preview) { ui_display_brightness_test_stop(); view->preview = false; }
    lv_obj_add_state(view->save, LV_STATE_DISABLED);
    lv_obj_add_state(view->cancel, LV_STATE_DISABLED);
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) lv_obj_add_state(view->controls[i], LV_STATE_DISABLED);
    for (int i = 0; i < SYSTEM_FIELD_COUNT; i++) lv_obj_add_state(view->system_controls[i], LV_STATE_DISABLED);
    ui_transition_close(view->root);
}

static void close_clicked(lv_event_t *event)
{
    settings_view_t *view = lv_event_get_user_data(event);
    if (!view->saving) close_view(view);
}

static void save_clicked(lv_event_t *event)
{
    settings_view_t *view = lv_event_get_user_data(event);
    if (view->saving || view->closing) return;
    system_collect(view);
    if (view->draft.dirty == 0) { close_view(view); return; }
    (void)screen_settings_status(true);
    if (!screen_settings_submit(&view->draft)) {
        lv_label_set_text(view->message, tr("failed", "Could not save. Try again."));
        return;
    }
    view->saving = true;
    lv_obj_add_state(view->save, LV_STATE_DISABLED);
    lv_obj_add_state(view->cancel, LV_STATE_DISABLED);
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) lv_obj_add_state(view->controls[i], LV_STATE_DISABLED);
    for (int i = 0; i < SYSTEM_FIELD_COUNT; i++) lv_obj_add_state(view->system_controls[i], LV_STATE_DISABLED);
    lv_label_set_text(view->message, tr("saving", "Saving..."));
}

static void result_tick(lv_timer_t *timer)
{
    settings_view_t *view = lv_timer_get_user_data(timer);
    if (!view->saving || view->closing) return;
    int result = screen_settings_status(false);
    if (result == 1) return;
    (void)screen_settings_status(true);
    if (result == 2) { close_view(view); return; }
    view->saving = false;
    lv_obj_remove_state(view->save, LV_STATE_DISABLED);
    lv_obj_remove_state(view->cancel, LV_STATE_DISABLED);
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) lv_obj_remove_state(view->controls[i], LV_STATE_DISABLED);
    for (int i = 0; i < SYSTEM_FIELD_COUNT; i++) lv_obj_remove_state(view->system_controls[i], LV_STATE_DISABLED);
    lv_label_set_text(view->message, tr("failed", "Could not save. Try again."));
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, lv_event_cb_t callback, settings_view_t *view)
{
    lv_obj_t *object = lv_button_create(parent);
    lv_obj_set_height(object, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(object, 10, 0);
    ui_style_button_auto(object, lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BG), lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BORDER));
    lv_obj_t *caption = label(object, text, UI_TEXT_LABEL);
    lv_obj_center(caption);
    if (callback != NULL) lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED, view);
    return object;
}

static void preview_clicked(lv_event_t *event)
{
    if (s_view == NULL || s_view->saving || s_view->closing) return;
    int field = (int)(uintptr_t)lv_event_get_user_data(event);
    s_view->preview = ui_display_brightness_test_start(s_view->draft.value[field], field == SCREEN_DIM ? 1 : 0);
}

void ui_screen_settings_open(void)
{
    if (s_view != NULL) { lv_obj_move_foreground(s_view->root); return; }
    if (screen_settings_status(true) == 1) return;
    settings_view_t *view = ui_calloc_prefer_psram(1, sizeof(*view));
    if (view == NULL) return;
    esp_err_t loaded = screen_settings_load(&view->draft);
    view->original = view->draft;
    s_view = view;
    view->root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(view->root);
    lv_obj_set_size(view->root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(view->root, ui_style_page_bg(lv_color_hex(APP_UI_COLOR_SCREEN_BG)), 0);
    lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(view->root, 12, 0);
    lv_obj_set_style_pad_row(view->root, 10, 0);
    lv_obj_set_flex_flow(view->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(view->root, false);
    lv_obj_add_event_cb(view->root, deleted, LV_EVENT_DELETE, view);
    lv_obj_t *header = container(view->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = label(header, tr("title", "System settings"), UI_TEXT_LABEL);
    lv_obj_set_flex_grow(title, 1);
    view->cancel = button(header, tr("cancel", "Cancel"), close_clicked, view);
    view->body = container(view->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(view->body, 0);
    lv_obj_set_flex_grow(view->body, 1);
    lv_obj_set_scrollable(view->body, true);
    lv_obj_set_scroll_dir(view->body, LV_DIR_VER);
    lv_obj_set_style_pad_all(view->body, 8, 0);
    static const char *system_keys[] = {"system.language", "system.timezone", "system.appearance", "system.theme", "system.page_animations"};
    static const char *system_names[] = {"Language", "Time zone", "Appearance", "Color theme", "Page animations"};
    for (int field = 0; field < SYSTEM_FIELD_COUNT; field++) {
        lv_obj_t *row = container(view->body, LV_FLEX_FLOW_COLUMN);
        lv_obj_t *caption = label(row, ui_i18n_get(system_keys[field], system_names[field]), UI_TEXT_LABEL);
        lv_obj_set_width(caption, lv_pct(100));
        lv_obj_t *control = view->system_controls[field] = lv_dropdown_create(row);
        size_t bytes = 1;
        for (unsigned i = 0; ; i++) {
            const char *id = system_id(view, field, i);
            if (id == NULL) break;
            bytes += strlen(system_name(view, field, id)) + 1;
        }
        char *options = ui_calloc_prefer_psram(bytes, 1);
        if (options != NULL) {
            char *cursor = options;
            for (unsigned i = 0; ; i++) {
                const char *id = system_id(view, field, i);
                if (id == NULL) break;
                if (i) *cursor++ = '\n';
                for (const char *name = system_name(view, field, id); *name; name++)
                    *cursor++ = *name == '\n' || *name == '\r' ? ' ' : *name;
            }
            lv_dropdown_set_options(control, options);
            free(options);
        } else { loaded = ESP_ERR_NO_MEM; lv_dropdown_set_options(control, ""); }
        lv_dropdown_set_selected(control, 0);
        lv_dropdown_set_symbol(control, NULL);
        lv_obj_set_width(control, lv_pct(100));
        ui_text_set(control, UI_TEXT_LABEL, false);
        lv_obj_t *list = lv_dropdown_get_list(control);
        ui_text_set(list, field == SYSTEM_TIMEZONE ? UI_TEXT_META : UI_TEXT_LABEL, false);
        lv_obj_set_style_max_width(list, lv_display_get_horizontal_resolution(lv_display_get_default()) - 40, 0);
        lv_obj_set_style_max_height(list, lv_pct(60), 0);
        lv_obj_add_event_cb(control, system_changed, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)field);
    }
    static const int low[] = {1, 1, 5, 0, 1, 15, 1, 0, 30, 0, 0, 0, 0, 0, 0, 0};
    static const int high[] = {100, 100, 3600, 4, 120, 7200, 100, 1, 7200, 1, 1, 1, 1, 3, 1, 1};
    for (int i = 0; i < SCREEN_FIELD_COUNT; i++) {
        lv_obj_t *row = view->rows[i] = container(view->body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(row, ui_style_control_bg(lv_color_hex(APP_UI_COLOR_TOPBAR_CHIP_BG)), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(row, 14, 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_t *caption = label(row, tr(keys[i], names[i]), UI_TEXT_LABEL);
        lv_obj_set_width(caption, lv_pct(100));
        lv_obj_t *control;
        if (i == SCREEN_XIAOZHI_EMOJI) {
            control = lv_dropdown_create(row);
            char options[160];
            snprintf(options, sizeof(options), "%s\n%s", ui_i18n_get("xiaozhi.emoji_otto", "Otto Robot"),
                ui_i18n_get("xiaozhi.emoji_classic", "Classic Emoji"));
            lv_dropdown_set_options(control, options);
            lv_dropdown_set_selected(control, view->draft.value[i]);
            lv_obj_set_width(control, lv_pct(100)); lv_dropdown_set_symbol(control, NULL);
            ui_text_set(control, UI_TEXT_LABEL, false);
            ui_text_set(lv_dropdown_get_list(control), UI_TEXT_LABEL, false);
        } else if (i == SCREEN_ROTATION) {
            control = lv_dropdown_create(row);
            lv_dropdown_set_options(control, "0°\n90°\n180°\n270°");
            lv_dropdown_set_selected(control, view->draft.value[i]);
            lv_obj_set_width(control, lv_pct(100));
            lv_dropdown_set_symbol(control, NULL);
            ui_text_set(control, UI_TEXT_LABEL, false);
            ui_text_set(lv_dropdown_get_list(control), UI_TEXT_LABEL, false);
        } else if (i == SCREEN_MODE) {
            control = lv_dropdown_create(row);
            char choices[256];
            snprintf(choices, sizeof(choices), "%s\n%s\n%s\n%s\n%s", tr("disabled", "Disabled"), tr("clock", "Clock"),
                tr("off", "Screen off"), tr("weather", "Weather"), tr("pages", "Cycle pages"));
            lv_dropdown_set_options(control, choices);
            lv_dropdown_set_selected(control, view->draft.value[i]);
            lv_obj_set_width(control, lv_pct(100));
            ui_text_set(control, UI_TEXT_LABEL, false);
            lv_dropdown_set_symbol(control, NULL);
            lv_obj_t *list = lv_dropdown_get_list(control);
            ui_text_set(list, UI_TEXT_LABEL, false);
        } else if (i == SCREEN_DIM_SECONDS || i == SCREEN_START_MINUTES || i == SCREEN_CLOCK_START || i == SCREEN_CLOCK_OFF_SECONDS) {
            static const int seconds[] = {5, 15, 30, 60, 120, 300, 600, 1800, 3600, 7200};
            static const int minutes[] = {1, 2, 5, 10, 15, 30, 60, 120};
            const int *presets = i == SCREEN_START_MINUTES ? minutes : seconds;
            int count = i == SCREEN_START_MINUTES ? 8 : 10;
            char options[100] = "";
            int used = 0, selected = -1;
            for (int j = 0; j < count; j++) if (presets[j] >= low[i] && presets[j] <= high[i]) {
                if (presets[j] == view->draft.value[i]) selected = used;
                view->choices[i][used++] = presets[j];
            }
            if (selected < 0) {
                selected = used;
                view->choices[i][used++] = view->draft.value[i];
            }
            size_t length = 0;
            for (int j = 0; j < used; j++) length += (size_t)snprintf(options + length, sizeof(options) - length,
                "%s%d", j ? "\n" : "", view->choices[i][j]);
            control = lv_dropdown_create(row);
            lv_dropdown_set_options(control, options);
            lv_dropdown_set_selected(control, selected);
            lv_obj_set_width(control, lv_pct(100));
            ui_text_set(control, UI_TEXT_LABEL, false);
            ui_text_set(lv_dropdown_get_list(control), UI_TEXT_LABEL, false);
            lv_dropdown_set_symbol(control, NULL);
        } else if (i == SCREEN_CLOCK_OFF || i >= SCREEN_CLOCK_24H) {
            control = lv_switch_create(row);
            lv_obj_set_size(control, 60, 32);
            if (view->draft.value[i]) lv_obj_add_state(control, LV_STATE_CHECKED);
        } else {
            view->values[i] = label(row, "", UI_TEXT_META);
            control = lv_slider_create(row);
            lv_obj_set_width(control, lv_pct(100));
            lv_obj_set_height(control, 16);
            lv_obj_set_style_margin_top(control, 8, 0);
            lv_obj_set_style_margin_bottom(control, 8, 0);
            lv_slider_set_range(control, low[i], high[i]);
            lv_slider_set_value(control, view->draft.value[i], LV_ANIM_OFF);
            lv_obj_t *preview = button(row, tr("preview", "Preview (30 s)"), NULL, view);
            lv_obj_add_event_cb(preview, preview_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
        view->controls[i] = control;
        lv_obj_add_event_cb(control, changed, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
    }
    lv_obj_t *footer = container(view->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    view->message = label(footer, tr("hint", "Changes take effect when saved."), UI_TEXT_META);
    lv_obj_set_flex_grow(view->message, 1);
    view->save = button(footer, tr("save", "Save"), save_clicked, view);
    if (loaded != ESP_OK) {
        lv_label_set_text(view->message, tr("load_failed", "Settings unavailable. Close and try again."));
        lv_obj_add_state(view->save, LV_STATE_DISABLED);
        lv_obj_set_hidden(view->body, true);
    }
    refresh(view);
    view->timer = lv_timer_create(result_tick, 100, view);
    ui_transition_fade_in(view->root);
}

lv_obj_t *ui_screen_settings_root(void) { return s_view != NULL ? s_view->root : NULL; }
