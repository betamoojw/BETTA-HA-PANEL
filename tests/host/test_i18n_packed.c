/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings/i18n_packed.h"
#include "settings/i18n_store.h"
#include "ui/ui_i18n.h"
#include "zlib.h"
#include "i18n_packed_catalogs.h"
#include "i18n/es.inc"
#include "i18n/fr.inc"
#include "i18n/pl.inc"
#include "i18n/zh-cn.inc"
#include "i18n/zh-tw.inc"
extern unsigned test_i18n_allocations, test_i18n_live, test_i18n_fail_at;
static bool custom_enabled;
static esp_err_t custom_translation(const char *code, char **out)
{
    *out = NULL;
    if (!custom_enabled || strcmp(code, "es")) return ESP_ERR_NOT_FOUND;
    *out = strdup("{\"lvgl\":{\"energy\":{\"water\":\"Custom water\"}}}");
    return *out ? ESP_OK : ESP_ERR_NO_MEM;
}
#define i18n_store_load_custom_translation custom_translation
#include "../../main/ui/ui_i18n.c"
#undef i18n_store_load_custom_translation
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "i18n packed line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void reject(const unsigned char *data, size_t size, size_t raw_size)
{
    CHECK(i18n_packed_parse(data, size, raw_size) == NULL);
    CHECK(test_i18n_live == 0);
}
static void *fail_json_alloc(size_t size) { (void)size; return NULL; }
int main(void)
{
    const char *original[] = {I18N_BUILTIN_ES, I18N_BUILTIN_FR, I18N_BUILTIN_PL, I18N_BUILTIN_ZH_CN, I18N_BUILTIN_ZH_TW};
    for (size_t i = 0; i < sizeof(original) / sizeof(original[0]); i++) {
        cJSON *expected = cJSON_ParseWithOpts(original[i], NULL, 1);
        cJSON *got = i18n_store_parse_builtin_translation(i18n_packed_catalogs[i].code);
        CHECK(expected && got && cJSON_Compare(expected, got, 1));
        CHECK(test_i18n_live == 0); /* Scratch is freed before the tree reaches its caller. */
        cJSON_Delete(expected); cJSON_Delete(got);
    }
    const unsigned char *data = i18n_packed_catalogs[0].data;
    size_t size = i18n_packed_catalogs[0].size, raw = i18n_packed_catalogs[0].json_size;
    unsigned char *changed = malloc(size + 1); CHECK(changed != NULL);
    memcpy(changed, data, size); changed[size] = 0;
    reject(NULL, size, raw); reject(data, 0, raw); reject(data, size, 0);
    reject(data, size, 32769); reject(data, 32769, raw);
    reject(data, size - 1, raw); reject(changed, size + 1, raw);
    reject(data, size, raw - 1); reject(data, size, raw + 1);
    changed[size - 1] ^= 1; reject(changed, size, raw); free(changed);
    for (unsigned failure = 1; failure <= 2; failure++) {
        test_i18n_fail_at = failure; test_i18n_allocations = 0;
        reject(data, size, raw);
    }
    test_i18n_fail_at = 0;
    cJSON_Hooks hooks = {.malloc_fn = fail_json_alloc, .free_fn = free};
    cJSON_InitHooks(&hooks); reject(data, size, raw); cJSON_InitHooks(NULL);
    const char invalid[][12] = {"[]", "{} garbage", "{}\0hidden", "bad JSON"};
    const size_t invalid_sizes[] = {2, 10, 9, 8};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        unsigned char packed[128]; uLongf length = sizeof(packed);
        CHECK(compress2(packed, &length, (const Bytef *)invalid[i], invalid_sizes[i], 9) == Z_OK);
        reject(packed, length, invalid_sizes[i]);
    }
    CHECK(ui_i18n_init("en") == ESP_OK);
    test_i18n_allocations = 0; test_i18n_fail_at = 1;
    CHECK(ui_i18n_init("es") == ESP_ERR_NO_MEM);
    CHECK(strcmp(ui_i18n_get_language(), "en") == 0);
    CHECK(strcmp(ui_i18n_get("energy.water", ""), "Water") == 0);
    CHECK(test_i18n_live == 0);
    test_i18n_fail_at = 0;
    CHECK(ui_i18n_init("es") == ESP_OK);
    CHECK(strcmp(ui_i18n_get("energy.water", ""), "Agua") == 0);
    custom_enabled = true;
    CHECK(ui_i18n_init("es") == ESP_OK);
    CHECK(strcmp(ui_i18n_get("energy.water", ""), "Custom water") == 0);
    CHECK(strcmp(ui_i18n_get("energy.gas", "missing"), "missing") != 0);
    custom_enabled = false;
    CHECK(ui_i18n_init("es") == ESP_OK);
    CHECK(strcmp(ui_i18n_get("energy.water", ""), "Agua") == 0);
    CHECK(test_i18n_live == 0);
    ui_i18n_reset();
    printf("test_i18n_packed: %u checks passed\n", checks);
    return 0;
}
