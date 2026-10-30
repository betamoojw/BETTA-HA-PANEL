/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/* The built-in DE/EN translation documents: strict JSON (no trailing data), one root object holding
 * only "lvgl", the same keys in both languages and a working ui_i18n_get() lookup. A surplus closing
 * brace once ended "lvgl" after "roborock": cJSON_Parse ignores trailing data, so "topbar", "energy"
 * and everything after them sat at the root and every lookup returned the English fallback. */
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "settings/i18n_store.h"
#include "ui/ui_i18n.h"

static int failures;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static cJSON *parse_strict(const char *lang)
{
    const char *json = i18n_store_builtin_translation_json(lang);
    CHECK(json != NULL, "%s: no built-in document", lang);
    if (json == NULL) {
        return NULL;
    }
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithOpts(json, &end, 1);
    CHECK(root != NULL, "%s: not strict JSON, parse stopped near offset %ld", lang,
          end != NULL ? (long)(end - json) : -1L);
    return root;
}

/* Every leaf of `a` must exist as a string leaf in `b`; returns the number of leaves in `a`. */
static int compare_leaves(const char *path, const cJSON *a, const cJSON *b, const char *a_name, const char *b_name)
{
    int count = 0;
    for (const cJSON *item = a->child; item != NULL; item = item->next) {
        for (const cJSON *previous = a->child; previous != item; previous = previous->next) {
            CHECK(strcmp(previous->string, item->string) != 0, "%s.%s: duplicate JSON member in %s",
                  path, item->string, a_name);
        }
        char next[160];
        snprintf(next, sizeof(next), "%s%s%s", path, path[0] != '\0' ? "." : "", item->string);
        const cJSON *other = cJSON_GetObjectItemCaseSensitive((cJSON *)b, item->string);
        if (cJSON_IsObject(item)) {
            CHECK(cJSON_IsObject(other), "%s is a section in %s but not in %s", next, a_name, b_name);
            if (cJSON_IsObject(other)) {
                count += compare_leaves(next, item, other, a_name, b_name);
            }
        } else {
            count++;
            CHECK(cJSON_IsString(item) && item->valuestring[0] != '\0', "%s: empty or not a string in %s", next, a_name);
            CHECK(cJSON_IsString(other), "%s is in %s but not in %s", next, a_name, b_name);
        }
    }
    return count;
}

static void test_documents(void)
{
    const char *codes[] = {"de", "es", "fr", "pl", "zh-cn", "zh-tw"};
    cJSON *en = parse_strict("en");
    if (en == NULL) return;
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        const char *code = codes[i];
        CHECK(i18n_store_is_builtin_language(code), "%s is not registered", code);
        cJSON *doc = parse_strict(code);
        if (doc == NULL) continue;
        const cJSON *body = cJSON_GetObjectItemCaseSensitive(doc, "lvgl");
        CHECK(cJSON_GetArraySize(doc) == 1 && cJSON_IsObject(body), "%s: invalid root", code);
        if (cJSON_IsObject(body))
            compare_leaves("", cJSON_GetObjectItemCaseSensitive(en, "lvgl"), body, "en", code);
        cJSON_Delete(doc);
    }
    cJSON_Delete(en);
}

static void test_lookup(void)
{
    struct {
        const char *lang;
        const char *key;
        const char *expected;
    } cases[] = {
        {"de", "topbar.ap", "AP"},
        {"de", "energy.title", "Energie"},
        {"de", "energy.water", "Wasser"},
        {"de", "energy.gas", "Gas"},
        {"de", "energy.autarky", "Autarkie"},
        {"de", "sensor.age.just_now", "gerade eben"},
        {"de", "boot.initializing_system", "System wird initialisiert"},
        {"en", "energy.title", "Energy"},
        {"en", "energy.water", "Water"},
        {"en", "energy.gas", "Gas"},
        {"en", "sensor.age.just_now", "just now"},
        {"en", "boot.initializing_system", "Initializing system"},
        {"es", "energy.water", "Agua"},
        {"fr", "energy.water", "Eau"},
        {"pl", "energy.water", "Woda"},
        {"zh-cn", "energy.grid", "电网"},
        {"zh-tw", "energy.grid", "電網"},
        {"zh-cn", "boot.initializing_system", "正在初始化系统"},
        {"zh-tw", "boot.initializing_system", "正在初始化系統"},
        {"de", "roborock.state.cleaning", "Reinigt"},
        {"de", "roborock.state.returning", "Kehrt zurück"},
        {"de", "roborock.state.paused", "Pausiert"},
        {"en", "roborock.state.cleaning", "Cleaning"},
        {"en", "roborock.state.docked", "Docked"},
        {"de", "ota.receiving", "Firmware wird empfangen ..."},
        {"en", "ota.receiving", "Receiving firmware..."},
        {"de", "solar.details_title", "Solarprognose heute"},
        {"en", "solar.details_title", "Solar forecast today"},
        {"de", "solar.remaining", "Verbleibend"},
        {"en", "solar.remaining", "Remaining"},
        {"de", "solar.total", "Gesamt"},
        {"en", "solar.total", "Total"},
        {"de", "solar.rest", "Rest"},
        {"en", "solar.rest", "Remaining"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CHECK(ui_i18n_init(cases[i].lang) == ESP_OK, "init %s", cases[i].lang);
        const char *got = ui_i18n_get(cases[i].key, "<<fallback>>");
        CHECK(strcmp(got, cases[i].expected) == 0, "%s %s: got \"%s\", want \"%s\"", cases[i].lang, cases[i].key, got,
              cases[i].expected);
    }
    ui_i18n_reset();
}

static void test_chinese_weekdays(void)
{
    const char *langs[] = {"zh-cn", "zh-tw"};
    for (size_t lang = 0; lang < 2; lang++) {
        CHECK(ui_i18n_init(langs[lang]) == ESP_OK, "weekday init");
        char labels[7][16];
        for (int day = 0; day < 7; day++) {
            char key[32];
            snprintf(key, sizeof(key), "weather.weekday_%d", day);
            ui_i18n_weekday_short(day, labels[day], sizeof(labels[day]));
            CHECK(strcmp(labels[day], ui_i18n_get(key, "")) == 0, "weekday %d", day);
            for (int prev = 0; prev < day; prev++)
                CHECK(strcmp(labels[day], labels[prev]) != 0, "distinct weekday %d", day);
            for (size_t capacity = 1; capacity <= 7; capacity++) {
                char tiny[8];
                memset(tiny, 0x7f, sizeof(tiny));
                ui_i18n_weekday_short(day, tiny, capacity);
                CHECK(strlen(tiny) == ((capacity - 1) / 3) * 3, "UTF-8 boundary %zu", capacity);
                CHECK(tiny[capacity] == 0x7f, "no overwrite %zu", capacity);
            }
        }
        char invalid[16];
        ui_i18n_weekday_short(99, invalid, sizeof(invalid));
        CHECK(strcmp(invalid, labels[0]) == 0, "invalid weekday defaults to Sunday");
        char sentinel = 'x';
        ui_i18n_weekday_short(0, &sentinel, 0);
        CHECK(sentinel == 'x', "zero capacity");
        ui_i18n_weekday_short(0, NULL, 4);
        CHECK(strcmp(ui_i18n_get("ota.title", ""), lang ? "韌體更新" : "固件更新") == 0, "OTA title");
        CHECK(strcmp(ui_i18n_get("titles.car", ""), lang ? "車輛" : "车辆") == 0, "car title");
    }
    ui_i18n_reset();
}

int main(void)
{
    test_documents();
    test_lookup();
    test_chinese_weekdays();
    if (failures != 0) {
        printf("test_i18n_builtin: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_i18n_builtin: ok\n");
    return 0;
}
