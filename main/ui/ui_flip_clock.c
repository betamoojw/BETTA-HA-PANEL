/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ui/ui_flip_clock.h"
#include "ui/ui_memory.h"
#include "ui/ui_i18n.h"
#include "ui/theme/theme_default.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/ui_typography.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    lv_obj_t *card, *half[4], *label[4], *seam;
    char value;
} flip_digit_t;

typedef struct {
    lv_obj_t *root, *group, *date, *period, *colon[2];
    lv_timer_t *timer;
    flip_digit_t digit[6];
    bool format_24h;
} flip_clock_t;

/* Two clipped static halves and two folding halves; only changed digits animate.
 * No full-screen snapshots or permanently allocated image buffers. */
static void flip_frame(void *var, int32_t progress)
{
    flip_digit_t *digit = var;
    lv_obj_set_hidden(digit->half[2], progress >= 128);
    lv_obj_set_hidden(digit->half[3], progress < 128 || progress >= 256);
    if (progress < 128) lv_obj_set_style_transform_scale_y(digit->half[2], 256 - 2 * progress, 0);
    else if (progress < 256) lv_obj_set_style_transform_scale_y(digit->half[3], LV_MAX(1, 2 * (progress - 128)), 0);
    else lv_label_set_text(digit->label[1], lv_label_get_text(digit->label[0]));
}

static void set_digit(flip_digit_t *digit, char value, bool animate)
{
    if (!animate) { lv_anim_delete(digit, flip_frame); flip_frame(digit, 256); }
    if (digit->value == value) return;
    lv_anim_delete(digit, flip_frame);
    char next[2] = {value, 0}, previous[2] = {digit->value ? digit->value : value, 0};
    lv_label_set_text(digit->label[0], next);
    lv_label_set_text(digit->label[1], previous);
    lv_label_set_text(digit->label[2], previous);
    lv_label_set_text(digit->label[3], next);
    bool flip = animate && digit->value && value != '-' && digit->value != '-';
    digit->value = value;
    if (!flip) { flip_frame(digit, 256); return; }
    flip_frame(digit, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, digit);
    lv_anim_set_exec_cb(&animation, flip_frame);
    lv_anim_set_values(&animation, 0, 256);
    lv_anim_set_duration(&animation, 320);
    lv_anim_start(&animation);
}

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_scrollable(object, false);
    lv_obj_set_clickable(object, false);
    return object;
}

static lv_obj_t *caption(lv_obj_t *parent, const char *text, ui_text_role_t role)
{
    lv_obj_t *label = lv_label_create(parent);
    ui_text_set(label, role, true);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, theme_default_color_text_muted(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_clickable(label, false);
    return label;
}

static void layout(flip_clock_t *clock)
{
    lv_obj_update_layout(clock->root);
    int width = lv_obj_get_content_width(clock->root) * 9 / 10;
    int height = lv_obj_get_content_height(clock->root);
    int gap = LV_MAX(4, width / 100);
    int period_width = clock->format_24h ? 0 : width / 13;
    int colon_width = width / 32;
    int digit_width = (width - period_width - 2 * colon_width - (clock->format_24h ? 7 : 8) * gap) / 6;
    int digit_height = LV_MIN(digit_width * 3 / 2, height / 2);
    digit_height -= digit_height % 2;
    const lv_font_t *font = ui_font_clock(digit_width * 6, digit_height - 8, true, true);
    const lv_font_t *date_font = ui_font_role(UI_TEXT_LABEL);
    int date_height = lv_font_get_line_height(date_font);
    int row_y = date_height + 2 * gap;
    lv_obj_set_size(clock->group, width, row_y + digit_height);
    lv_obj_center(clock->group);
    lv_obj_set_size(clock->date, width, date_height);
    lv_obj_set_pos(clock->date, 0, 0);
    lv_obj_set_style_text_font(clock->date, date_font, 0);
    int x = 0;
    for (unsigned i = 0; i < 6; i++) {
        flip_digit_t *digit = &clock->digit[i];
        lv_anim_delete(digit, flip_frame);
        flip_frame(digit, 256);
        lv_obj_set_pos(digit->card, x, row_y);
        lv_obj_set_size(digit->card, digit_width, digit_height);
        int edge = lv_obj_get_style_border_width(digit->card, 0);
        int inner_width = digit_width - 2 * edge, inner_height = digit_height - 2 * edge;
        for (unsigned part = 0; part < 4; part++) {
            bool bottom = part & 1U;
            lv_obj_set_size(digit->half[part], inner_width, inner_height / 2);
            lv_obj_set_pos(digit->half[part], 0, bottom ? inner_height / 2 : 0);
            lv_obj_set_style_transform_pivot_y(digit->half[part], bottom ? 0 : inner_height / 2, 0);
            lv_obj_set_style_text_font(digit->label[part], font, 0);
            lv_obj_set_width(digit->label[part], inner_width);
            lv_obj_set_pos(digit->label[part], 0,
                (inner_height - lv_font_get_line_height(font)) / 2 - (bottom ? inner_height / 2 : 0));
        }
        lv_obj_set_size(digit->seam, inner_width, 2);
        lv_obj_set_pos(digit->seam, 0, inner_height / 2 - 1);
        x += digit_width + gap;
        if (i == 1 || i == 3) {
            lv_obj_t *colon = clock->colon[i / 2];
            const lv_font_t *colon_font = ui_text_fit(":", ui_font_role(UI_TEXT_HERO), colon_width, digit_height);
            lv_obj_set_style_text_font(colon, colon_font, 0);
            lv_obj_set_size(colon, colon_width, lv_font_get_line_height(colon_font));
            lv_obj_set_pos(colon, x, row_y + (digit_height - lv_font_get_line_height(colon_font)) / 2);
            x += colon_width + gap;
        }
    }
    lv_obj_set_hidden(clock->period, clock->format_24h);
    if (!clock->format_24h) {
        const lv_font_t *period_font = ui_text_fit("AM", ui_font_role(UI_TEXT_LABEL), period_width, digit_height);
        period_font = ui_text_fit("PM", period_font, period_width, digit_height);
        lv_obj_set_style_text_font(clock->period, period_font, 0);
        lv_obj_set_size(clock->period, period_width, lv_font_get_line_height(period_font));
        lv_obj_set_pos(clock->period, width - period_width, row_y + (digit_height - lv_font_get_line_height(period_font)) / 2);
    }
    /* Resolve new bounds before the first date fit; otherwise its width is zero. */
    lv_obj_update_layout(clock->group);
    lv_obj_set_style_text_font(clock->date, ui_text_fit(lv_label_get_text(clock->date),
        date_font, width, date_height), 0);
}

void ui_flip_clock_update(lv_obj_t *object, const struct tm *time, bool animate)
{
    if (!object) return;
    flip_clock_t *clock = lv_obj_get_user_data(object);
    bool valid = time && time->tm_year >= 124 && time->tm_hour >= 0 && time->tm_hour < 24 &&
        time->tm_min >= 0 && time->tm_min < 60 && time->tm_sec >= 0 && time->tm_sec <= 60 &&
        time->tm_wday >= 0 && time->tm_wday < 7;
    char digits[16] = "------", date[128] = "---- -- --", period[3] = "";
    if (valid) {
        int hour = clock->format_24h ? time->tm_hour : (time->tm_hour % 12 ? time->tm_hour % 12 : 12);
        snprintf(digits, sizeof(digits), "%02d%02d%02d", hour, time->tm_min, time->tm_sec);
        snprintf(period, sizeof(period), "%s", time->tm_hour < 12 ? "AM" : "PM");
        static const char *const weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        char key[32];
        snprintf(key, sizeof(key), "weather.day_full_%d", time->tm_wday);
        snprintf(date, sizeof(date), "%s, %04d-%02d-%02d", ui_i18n_get(key, weekdays[time->tm_wday]),
            time->tm_year + 1900, time->tm_mon + 1, time->tm_mday);
    }
    for (unsigned i = 0; i < 6; i++) set_digit(&clock->digit[i], digits[i], animate);
    if (strcmp(lv_label_get_text(clock->date), date)) {
        lv_obj_set_style_text_font(clock->date, ui_text_fit(date, ui_font_role(UI_TEXT_LABEL),
            lv_obj_get_width(clock->date), lv_obj_get_height(clock->date)), 0);
        lv_label_set_text(clock->date, date);
    }
    if (strcmp(lv_label_get_text(clock->period), period)) lv_label_set_text(clock->period, period);
}

static void tick(lv_timer_t *timer)
{
    lv_obj_t *object = lv_timer_get_user_data(timer);
    if (!lv_obj_is_visible(object)) return;
    time_t now = time(NULL);
    struct tm local = {0};
    ui_flip_clock_update(object, localtime_r(&now, &local), true);
}

void ui_flip_clock_set_format(lv_obj_t *object, bool format_24h)
{
    if (!object) return;
    flip_clock_t *clock = lv_obj_get_user_data(object);
    if (clock->format_24h == format_24h) return;
    clock->format_24h = format_24h;
    layout(clock);
    for (unsigned i = 0; i < 6; i++) clock->digit[i].value = 0;
    if (clock->timer) tick(clock->timer);
}

static void deleted(lv_event_t *event)
{
    flip_clock_t *clock = lv_event_get_user_data(event);
    if (clock->timer) lv_timer_delete(clock->timer);
    for (unsigned i = 0; i < 6; i++) lv_anim_delete(&clock->digit[i], flip_frame);
    free(clock);
}

lv_obj_t *ui_flip_clock_create(lv_obj_t *parent, bool format_24h)
{
    flip_clock_t *clock = ui_calloc_prefer_psram(1, sizeof(*clock));
    if (!clock) return NULL;
    clock->format_24h = format_24h;
    clock->root = plain(parent);
    lv_obj_set_size(clock->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_user_data(clock->root, clock);
    lv_obj_add_event_cb(clock->root, deleted, LV_EVENT_DELETE, clock);
    clock->group = plain(clock->root);
    clock->date = caption(clock->group, "", UI_TEXT_LABEL);
    clock->period = caption(clock->group, "", UI_TEXT_LABEL);
    for (unsigned i = 0; i < 2; i++) clock->colon[i] = caption(clock->group, ":", UI_TEXT_HERO);
    for (unsigned i = 0; i < 6; i++) {
        flip_digit_t *digit = &clock->digit[i];
        digit->card = plain(clock->group);
        theme_default_style_card(digit->card);
        ui_style_apply_card(digit->card);
        lv_obj_set_style_pad_all(digit->card, 0, 0);
        lv_obj_set_style_clip_corner(digit->card, true, 0);
        lv_color_t surface = lv_obj_get_style_bg_color(digit->card, 0);
        lv_color_t shade = lv_obj_get_style_bg_grad_color(digit->card, 0);
        lv_grad_dir_t gradient = lv_obj_get_style_bg_grad_dir(digit->card, 0);
        for (unsigned part = 0; part < 4; part++) {
            digit->half[part] = plain(digit->card);
            lv_obj_set_style_bg_opa(digit->half[part], part < 2 ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(digit->half[part], surface, 0);
            if (part >= 2 && gradient == LV_GRAD_DIR_VER) {
                lv_color_t middle = lv_color_mix(surface, shade, 128);
                lv_obj_set_style_bg_color(digit->half[part], part == 2 ? surface : middle, 0);
                lv_obj_set_style_bg_grad_color(digit->half[part], part == 2 ? middle : shade, 0);
                lv_obj_set_style_bg_grad_dir(digit->half[part], LV_GRAD_DIR_VER, 0);
            }
            digit->label[part] = caption(digit->half[part], "", UI_TEXT_HERO);
            lv_obj_set_style_text_color(digit->label[part], theme_default_color_text_primary(), 0);
        }
        digit->seam = plain(digit->card);
        lv_obj_set_style_bg_color(digit->seam, ui_style_page_bg(lv_color_hex(APP_UI_COLOR_CONTENT_BG)), 0);
        lv_obj_set_style_bg_opa(digit->seam, LV_OPA_70, 0);
    }
    layout(clock);
    clock->timer = lv_timer_create(tick, 100, clock->root);
    if (!clock->timer) {
        lv_obj_delete(clock->root);
        return NULL;
    }
    time_t now = time(NULL);
    struct tm local = {0};
    ui_flip_clock_update(clock->root, localtime_r(&now, &local), false);
    return clock->root;
}
