/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi/xiaozhi_custom_wake.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct model_iface_data_t { bool alive; };
static struct model_iface_data_t instance;
static srmodel_list_t catalog;
static esp_mn_results_t result;
static esp_mn_error_t invalid_commands;
static const char *resource_path;
static bool available = true, fst = true, reject, no_result;
static int catalogs, creates, cleans;
static esp_mn_state_t state;
static float threshold;

bool xiaozhi_custom_model_available(void) { return available; }
const char *xiaozhi_custom_model_path(void) { return resource_path; }
srmodel_list_t *srmodel_load(const void *p)
{ assert(p && !catalogs); ++catalogs; return &catalog; }
void esp_srmodel_deinit(srmodel_list_t *p)
{ assert(p == &catalog && catalogs == 1 && !instance.alive); --catalogs; }
char *esp_srmodel_filter(srmodel_list_t *p, const char *prefix, const char *name)
{
    assert(p == &catalog);
    if (!strcmp(prefix, "fst")) return fst ? "fst" : NULL;
    assert(!strcmp(prefix, "mn") && !strcmp(name, "mn7_cn")); return "mn7_cn";
}
static model_iface_data_t *create(const char *name, int duration)
{ assert(!strcmp(name, "mn7_cn") && duration == 3000 && !instance.alive); ++creates; instance.alive = true; return &instance; }
static void destroy(model_iface_data_t *p) { assert(p->alive); p->alive = false; }
static int rate(model_iface_data_t *p) { assert(p->alive); return 16000; }
static int chunk(model_iface_data_t *p) { assert(p->alive); return 512; }
static int set_threshold(model_iface_data_t *p, float value) { assert(p->alive); threshold = value; return 0; }
static esp_mn_state_t detect(model_iface_data_t *p, int16_t *samples)
{ assert(p->alive && samples); return state; }
static esp_mn_results_t *results(model_iface_data_t *p)
{ assert(p->alive); return no_result ? NULL : &result; }
static void clean(model_iface_data_t *p) { assert(p->alive); ++cleans; }
static esp_mn_iface_t iface = {.create=create, .destroy=destroy, .get_samp_rate=rate,
    .get_samp_chunksize=chunk, .set_det_threshold=set_threshold, .detect=detect,
    .get_results=results, .clean=clean};
esp_mn_iface_t *esp_mn_handle_from_name(char *name) { assert(!strcmp(name, "mn7_cn")); return &iface; }
esp_err_t esp_mn_commands_clear(void) { assert(instance.alive); return ESP_OK; }
esp_err_t esp_mn_commands_add(int id, const char *phrase)
{ assert(instance.alive && id == 1 && !strcmp(phrase, "ni hao xiao bei")); return ESP_OK; }
esp_mn_error_t *esp_mn_commands_update(void) { return reject ? &invalid_commands : NULL; }

int main(int argc, char **argv)
{
    assert(argc == 2); resource_path = argv[1];
    available = false; assert(!xiaozhi_custom_wake_init("ni hao xiao bei")); available = true;
    fst = false; assert(!xiaozhi_custom_wake_init("ni hao xiao bei") && !creates && !catalogs); fst = true;
    reject = true; assert(!xiaozhi_custom_wake_init("ni hao xiao bei") && !instance.alive && !catalogs); reject = false;
    assert(xiaozhi_custom_wake_init("ni hao xiao bei"));
    int count = creates;
    assert(xiaozhi_custom_wake_init("ni hao xiao bei") && creates == count);
    assert(xiaozhi_custom_wake_chunk() == 512);
    int16_t pcm[512] = {0};
    state = ESP_MN_STATE_DETECTING; assert(!xiaozhi_custom_wake_detect(pcm) && !cleans);
    state = ESP_MN_STATE_TIMEOUT; assert(!xiaozhi_custom_wake_detect(pcm) && cleans == 1);
    /* Regression: the first on-device phrase scored .319. It must not be
     * rejected by a second .90 gate after MultiNet has accepted it. */
    state = ESP_MN_STATE_DETECTED;
    result.num = 1; result.command_id[0] = 1; result.prob[0] = .319f;
    assert(threshold <= result.prob[0] && threshold > .1f);
    assert(xiaozhi_custom_wake_detect(pcm));
    result.prob[0] = .1f; assert(!xiaozhi_custom_wake_detect(pcm));
    result.prob[0] = .9f; result.command_id[0] = 2; assert(!xiaozhi_custom_wake_detect(pcm));
    result.command_id[0] = 1; result.num = 0; assert(!xiaozhi_custom_wake_detect(pcm));
    no_result = true; assert(!xiaozhi_custom_wake_detect(pcm));
    xiaozhi_custom_wake_free(); xiaozhi_custom_wake_free();
    assert(!instance.alive && !catalogs && !xiaozhi_custom_wake_chunk());
    assert(!xiaozhi_custom_wake_detect(pcm));
    puts("Custom wake: resource preflight, lifecycle, rejection and measured-score regression passed");
}
