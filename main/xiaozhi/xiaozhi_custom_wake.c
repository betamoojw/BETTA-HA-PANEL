/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_custom_wake.h"
#include "xiaozhi_custom_model_store.h"
#include "xiaozhi_custom_resource.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static void *blob;
static srmodel_list_t *models;
static esp_mn_iface_t *iface;
static model_iface_data_t *detector;
static char active[XIAOZHI_WAKE_PHRASE_SIZE];
/* Match the custom MultiNet path's upstream default. WakeNet's preset
 * operating-point numbers are not MultiNet confidence thresholds. */
static const float detection_threshold = 0.2f;

void xiaozhi_custom_wake_free(void)
{
    /* MultiNet owns its global command list and releases it on destroy. */
    if (detector) iface->destroy(detector);
    detector = NULL; iface = NULL;
    if (models) esp_srmodel_deinit(models);
    models = NULL;
    heap_caps_free(blob); blob = NULL; active[0] = 0;
}
bool xiaozhi_custom_wake_init(const char *phrase)
{
    if (!xiaozhi_wake_phrase_valid(phrase) || !xiaozhi_custom_model_available()) return false;
    if (detector && !strcmp(active, phrase)) return true;
    xiaozhi_custom_wake_free();
    blob = heap_caps_malloc(XIAOZHI_CUSTOM_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!blob) return false;
    FILE *f = fopen(xiaozhi_custom_model_path(), "rb");
    bool ok = f && fread(blob, 1, XIAOZHI_CUSTOM_BYTES, f) == XIAOZHI_CUSTOM_BYTES;
    if (f) fclose(f);
    if (!ok) goto error;
    models = srmodel_load(blob);
    char *name = models ? esp_srmodel_filter(models, ESP_MN_PREFIX, "mn7_cn") : NULL;
    /* ESP-SR 2.4.7 model_init dereferences a failed model_create result.
     * Its required seed FST must exist before entering the vendor code. */
    if (!name || !esp_srmodel_filter(models, "fst", NULL)) goto error;
    iface = esp_mn_handle_from_name(name);
    if (iface) detector = iface->create(name, 3000);
    if (!detector || iface->get_samp_rate(detector) != 16000 ||
        xiaozhi_custom_wake_chunk() <= 0 || xiaozhi_custom_wake_chunk() > 1920) goto error;
    iface->set_det_threshold(detector, detection_threshold);
    if (esp_mn_commands_clear() != ESP_OK || esp_mn_commands_add(1, phrase) != ESP_OK ||
        esp_mn_commands_update() != NULL) goto error;
    strcpy(active, phrase);
    return true;
error:
    xiaozhi_custom_wake_free(); return false;
}
int xiaozhi_custom_wake_chunk(void) { return detector ? iface->get_samp_chunksize(detector) : 0; }
bool xiaozhi_custom_wake_detect(int16_t *mono)
{
    if (!detector) return false;
    esp_mn_state_t state = iface->detect(detector, mono);
    bool hit = false;
    if (state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *r = iface->get_results(detector);
        hit = r && r->num > 0 && r->command_id[0] == 1 && r->prob[0] >= detection_threshold;
        if (hit) ESP_LOGI("xiaozhi", "Custom wake accepted (confidence=%d/1000)", (int)(r->prob[0] * 1000));
    }
    if (state != ESP_MN_STATE_DETECTING) iface->clean(detector);
    return hit;
}
