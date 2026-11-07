/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "ui/ui_flip_clock.h"
#include "ui/ui_i18n.h"
#include "ui/theme/theme_default.h"
#include "ui/theme/ui_style.h"
#include "ui/theme/ui_typography.h"
#include "ui/text_font.h"

static unsigned checks;
static uint32_t ticks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "Flip clock line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static uint32_t now(void) { return ticks; }
static unsigned timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) count++;
    return count;
}
static void expect_digits(lv_obj_t *group, const char *text)
{
    for (unsigned i = 0; i < 6; i++) {
        lv_obj_t *card = lv_obj_get_child(group, 4 + i);
        for (unsigned half = 0; half < 2; half++) {
            const char *digit = lv_label_get_text(lv_obj_get_child(lv_obj_get_child(card, half), 0));
            CHECK(digit[0] == text[i] && digit[1] == 0);
        }
    }
}
static void bounds(lv_obj_t *group)
{
    lv_obj_update_layout(group);
    lv_area_t outer; lv_obj_get_coords(lv_obj_get_parent(group), &outer);
    lv_area_t inner; lv_obj_get_coords(group, &inner);
    CHECK(abs((inner.x1 + inner.x2) - (outer.x1 + outer.x2)) <= 2);
    CHECK(abs((inner.y1 + inner.y2) - (outer.y1 + outer.y2)) <= 2);
    for (uint32_t i = 0; i < lv_obj_get_child_count(group); i++) {
        lv_obj_t *child = lv_obj_get_child(group, i);
        /* The test parent can exceed this binary's display; still audit its captions. */
        if (lv_obj_is_hidden(child)) continue;
        lv_area_t area; lv_obj_get_coords(child, &area);
        CHECK(area.x1 >= inner.x1 && area.x2 <= inner.x2 && area.y1 >= inner.y1 && area.y2 <= inner.y2);
        if (i < 4) {
            lv_point_t size;
            lv_text_get_size(&size, lv_label_get_text(child), lv_obj_get_style_text_font(child, 0), 0, 0, LV_COORD_MAX, 0);
            if (size.x > lv_obj_get_width(child) || size.y > lv_obj_get_height(child))
                fprintf(stderr, "Clock caption %u '%s': text %ldx%ld, box %ldx%ld\n", (unsigned)i,
                    lv_label_get_text(child), (long)size.x, (long)size.y,
                    (long)lv_obj_get_width(child), (long)lv_obj_get_height(child));
            CHECK(size.x <= lv_obj_get_width(child) && size.y <= lv_obj_get_height(child));
        } else {
            /* The halves deliberately clip vertically; the complete digit must
             * still fit inside the card, including Matte's border. */
            lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(child, 0), 0);
            lv_point_t size;
            lv_text_get_size(&size, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0), 0, 0, LV_COORD_MAX, 0);
            int border = lv_obj_get_style_border_width(child, 0);
            CHECK(size.x <= lv_obj_get_width(child) - 2 * border);
            CHECK(size.y <= lv_obj_get_height(child) - 2 * border);
        }
    }
}

static void compact_locales(void)
{
    static const struct {
        const char *language;
        const char *weekdays[7];
    } locales[] = {
        {"en", {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}},
        {"de", {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"}},
        {"es", {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"}},
        {"fr", {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"}},
        {"pl", {"Niedziela", "Poniedziałek", "Wtorek", "Środa", "Czwartek", "Piątek", "Sobota"}},
        {"zh-cn", {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"}},
        {"zh-tw", {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"}},
    };
    unsigned timers = timer_count();
    /* Dashboard and full-screen saver, including the S3's larger text roles. */
    for (int height = 360; height <= 480; height += 120) {
        for (unsigned language = 0; language < sizeof(locales) / sizeof(locales[0]); language++) {
            CHECK(ui_i18n_init(locales[language].language) == ESP_OK);
            for (int step = 0; step <= 2; step++) {
                ui_fonts_set_user_step(step);
                for (int format = 0; format < 2; format++) {
                    lv_obj_t *parent = lv_obj_create(lv_screen_active());
                    lv_obj_remove_style_all(parent);
                    lv_obj_set_size(parent, 480, height);
                    lv_obj_t *clock = ui_flip_clock_create(parent, format != 0);
                    CHECK(clock && timer_count() == timers + 1);
                    for (int day = 0; day < 7; day++) {
                        struct tm sample = {.tm_year=126,.tm_mon=10,.tm_mday=1+day,.tm_wday=day,
                                            .tm_hour=23,.tm_min=58,.tm_sec=49};
                        ui_flip_clock_update(clock, &sample, false);
                        lv_obj_t *group = lv_obj_get_child(clock, 0);
                        expect_digits(group, format ? "235849" : "115849");
                        char expected_date[96];
                        snprintf(expected_date, sizeof(expected_date), "%s, 2026-11-%02d",
                                 locales[language].weekdays[day], day + 1);
                        CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(group, 0)), expected_date));
                        CHECK(lv_obj_is_hidden(lv_obj_get_child(group, 1)) == (format != 0));
                        CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(group, 1)), "PM"));
                        bounds(group);
                    }
                    lv_obj_delete(parent);
                    CHECK(timer_count() == timers && lv_anim_count_running() == 0);
                }
            }
        }
    }
}

void sim_flip_clock_regression(void)
{
    const int sizes[][2] = {{480,360},{480,480},{720,600},{1280,680},{1280,600},{720,1160},{800,1160}};
    const char *themes[] = {"dark_v2", "light", "black", "contrast"};
    char previous_theme[64], previous_language[16];
    snprintf(previous_theme, sizeof(previous_theme), "%s", theme_palette_active_id());
    snprintf(previous_language, sizeof(previous_language), "%s", ui_i18n_get_language());
    ui_style_kind_t previous_style = ui_style_get();
    int previous_step = ui_fonts_user_step();
    ticks = lv_tick_get(); lv_tick_set_cb(now);
    unsigned baseline_timers = timer_count();
    for (unsigned shape = 0; shape < sizeof(sizes)/sizeof(sizes[0]); shape++) {
        for (unsigned theme = 0; theme < sizeof(themes)/sizeof(themes[0]); theme++) {
            CHECK(theme_palette_activate_by_id(themes[theme]) == ESP_OK);
            theme_default_rebuild_styles();
            for (int style = UI_STYLE_FLAT; style <= UI_STYLE_MATTE; style++) {
                ui_style_set((ui_style_kind_t)style);
                CHECK(ui_i18n_init(theme == 1 ? "zh-cn" : "en") == ESP_OK);
                for (int format = 0; format < 2; format++) {
                    ui_fonts_set_user_step(format ? 2 : 0);
                    lv_obj_t *parent = lv_obj_create(lv_screen_active());
                    lv_obj_remove_style_all(parent);
                    lv_obj_set_size(parent, sizes[shape][0], sizes[shape][1]);
                    lv_obj_t *clock = ui_flip_clock_create(parent, format != 0);
                    CHECK(clock != NULL && timer_count() == baseline_timers + 1);
                    lv_obj_t *group = lv_obj_get_child(clock, 0);
                    struct tm time = {.tm_year=126,.tm_mon=9,.tm_mday=31,.tm_wday=6,.tm_hour=0,.tm_min=0,.tm_sec=0};
                    ui_flip_clock_update(clock, &time, false);
                    expect_digits(group, format ? "000000" : "120000");
                    CHECK(strstr(lv_label_get_text(lv_obj_get_child(group, 0)), "2026-10-31") != NULL);
                    lv_obj_t *date = lv_obj_get_child(group, 0);
                    CHECK(lv_obj_get_style_text_font(date, 0) == ui_text_fit(lv_label_get_text(date),
                        ui_font_role(UI_TEXT_LABEL), lv_obj_get_width(date), lv_obj_get_height(date)));
                    CHECK(lv_obj_is_hidden(lv_obj_get_child(group, 1)) == (format != 0));
                    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(group, 1)), "AM"));
                    bounds(group);
                    time.tm_hour = 12; time.tm_sec = 59;
                    ui_flip_clock_update(clock, &time, false);
                    expect_digits(group, "120059");
                    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(group, 1)), "PM"));
                    time.tm_hour = 23;
                    ui_flip_clock_update(clock, &time, false);
                    expect_digits(group, format ? "230059" : "110059");
                    ui_flip_clock_set_format(clock, format == 0);
                    ui_flip_clock_update(clock, &time, false);
                    CHECK(lv_obj_get_style_text_font(date, 0) == ui_text_fit(lv_label_get_text(date),
                        ui_font_role(UI_TEXT_LABEL), lv_obj_get_width(date), lv_obj_get_height(date)));
                    expect_digits(group, format ? "110059" : "230059");
                    bounds(group);
                    ui_flip_clock_update(clock, NULL, false);
                    expect_digits(group, "------");
                    time.tm_hour = 23; time.tm_min = 59; time.tm_sec = 59;
                    ui_flip_clock_update(clock, &time, false);
                    time.tm_mon = 10; time.tm_mday = 1; time.tm_wday = 0;
                    time.tm_hour = 0; time.tm_min = 0; time.tm_sec = 0;
                    ui_flip_clock_update(clock, &time, true);
                    CHECK(lv_anim_count_running() > 0);
                    ticks += 80; lv_anim_refr_now();
                    lv_obj_t *fold = lv_obj_get_child(lv_obj_get_child(group, 9), 2);
                    CHECK(!lv_obj_is_hidden(fold) && lv_obj_get_style_transform_scale_y(fold, 0) < 256);
                    ticks += 160; lv_anim_refr_now();
                    lv_obj_t *lower_fold = lv_obj_get_child(lv_obj_get_child(group, 9), 3);
                    CHECK(lv_obj_is_hidden(fold) && !lv_obj_is_hidden(lower_fold));
                    CHECK(lv_obj_get_style_transform_scale_y(lower_fold, 0) > 0 &&
                        lv_obj_get_style_transform_scale_y(lower_fold, 0) < 256);
                    ticks += 140; lv_anim_refr_now();
                    expect_digits(group, format ? "120000" : "000000");
                    CHECK(strstr(lv_label_get_text(lv_obj_get_child(group, 0)), "2026-11-01") != NULL);
                    time.tm_sec = 1; ui_flip_clock_update(clock, &time, true);
                    CHECK(lv_anim_count_running() > 0);
                    ui_flip_clock_update(clock, &time, false); /* settle an unchanged target */
                    CHECK(lv_anim_count_running() == 0);
                    expect_digits(group, format ? "120001" : "000001");
                    time.tm_sec = 2; ui_flip_clock_update(clock, &time, true);
                    CHECK(lv_anim_count_running() > 0);
                    lv_obj_delete(parent); /* reset/restore during the flip */
                    CHECK(lv_anim_count_running() == 0 && timer_count() == baseline_timers);
                }
            }
        }
    }
    compact_locales();
    ui_fonts_set_user_step(previous_step);
    CHECK(theme_palette_activate_by_id(previous_theme) == ESP_OK);
    ui_style_set(previous_style); theme_default_rebuild_styles();
    CHECK(ui_i18n_init(previous_language) == ESP_OK);
    lv_tick_set_cb(SDL_GetTicks);
    printf("Flip clock: %u checks passed (geometry, themes, formats, rollover, animation and cleanup)\n", checks);
}
