/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
 * WakeNet lifecycle follows EspWakeWord in betamoojw/xiaozhi-esp32 feat/knx_ip.
 * Models and inference are supplied by the pinned Espressif ESP-SR dependency.
 */
#include "xiaozhi_wake.h"
#include <stddef.h>
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_vad.h"
#include "model_path.h"

extern const unsigned char model_start[] asm("_binary_xiaozhi_wake_model_bin_start");
static srmodel_list_t *models;
static const esp_wn_iface_t *iface;
static model_iface_data_t *detector;
static vad_handle_t vad;

bool xiaozhi_wake_init(void)
{
    if (detector) return true;
    models = srmodel_load(model_start);
    char *name = models ? esp_srmodel_filter(models, ESP_WN_PREFIX, "nihaoxiaozhi") : NULL;
    iface = name ? esp_wn_handle_from_name(name) : NULL;
    if (iface) detector = iface->create(name, DET_MODE_95);
    vad = vad_create(VAD_MODE_3);
    if (!detector || !vad || iface->get_samp_rate(detector) != 16000 ||
        iface->get_channel_num(detector) != 1 ||
        xiaozhi_wake_chunk() <= 0 || xiaozhi_wake_chunk() > 1920) {
        xiaozhi_wake_free(); return false;
    }
    return true;
}
int xiaozhi_wake_chunk(void) { return detector ? iface->get_samp_chunksize(detector) : 0; }
bool xiaozhi_wake_detect(int16_t *mono) { return detector && iface->detect(detector, mono) > 0; }
bool xiaozhi_voice_active(int16_t *mono, int samples)
{
    bool speech = false;
    for (int i = 0; vad && i + 480 <= samples; i += 480)
        speech |= vad_process(vad, mono + i, 16000, 30) == VAD_SPEECH;
    return speech;
}
void xiaozhi_wake_reset(void)
{
    /* ESP-SR 2.4.7/P4 WakeNet9 clean walks nine layer pointers although
     * create/destroy use six. Recreate on rearm; never call that clean hook. */
    xiaozhi_wake_free();
}
void xiaozhi_wake_free(void)
{
    if (detector) iface->destroy(detector);
    detector = NULL;
    if (vad) vad_destroy(vad);
    vad = NULL;
    if (models) esp_srmodel_deinit(models);
    models = NULL; iface = NULL;
}
