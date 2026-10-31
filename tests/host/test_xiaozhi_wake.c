/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xiaozhi/xiaozhi_wake.h"
#include "xiaozhi/xiaozhi_pcm.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_vad.h"
#include "model_path.h"

const unsigned char model_blob[] asm("_binary_xiaozhi_wake_model_bin_start") = {1};
void *xiaozhi_wake_model_load(int word) { assert(word == 0); return (void *)model_blob; }
void xiaozhi_wake_model_free(void *p) { assert(!p || p == model_blob); }
bool xiaozhi_custom_wake_init(const char *p) { (void)p; return false; }
void xiaozhi_custom_wake_free(void) {}
int xiaozhi_custom_wake_chunk(void) { return 0; }
bool xiaozhi_custom_wake_detect(int16_t *p) { (void)p; return false; }
struct model_iface_data_t { int alive; };
static struct model_iface_data_t instance;
static srmodel_list_t catalog;
static vad_handle_with_trigger_t vad_instance;
static int live_models, live_vad, created, destroyed, rate = 16000, channels = 1;
static bool fail_model, fail_vad;
srmodel_list_t *srmodel_load(const void *p)
{ assert(p == model_blob && !live_models); ++live_models; return &catalog; }
void esp_srmodel_deinit(srmodel_list_t *p)
{ assert(p == &catalog && live_models == 1 && !instance.alive); --live_models; }
char *esp_srmodel_filter(srmodel_list_t *p, const char *a, const char *b)
{ assert(p == &catalog && !strcmp(a, "wn") && !strcmp(b, "wn9_nihaoxiaozhi")); return "wn9_nihaoxiaozhi"; }
static model_iface_data_t *create(const void *name, det_mode_t mode)
{
    assert(!strcmp(name, "wn9_nihaoxiaozhi") && mode == DET_MODE_95 && !instance.alive);
    if (fail_model) return NULL;
    ++created; instance.alive = 1; return &instance;
}
static int sample_rate(model_iface_data_t *p) { assert(p->alive); return rate; }
static int channel_count(model_iface_data_t *p) { assert(p->alive); return channels; }
static int chunk(model_iface_data_t *p) { assert(p->alive); return 512; }
static wakenet_state_t detect(model_iface_data_t *p, int16_t *s)
{ assert(p->alive); return s[0] == 123 ? WAKENET_DETECTED : WAKENET_NO_DETECT; }
static void clean(model_iface_data_t *p) { (void)p; assert(!"unsafe clean hook called"); }
static void destroy(model_iface_data_t *p)
{ assert(p == &instance && p->alive); p->alive = 0; ++destroyed; }
static const esp_wn_iface_t mock = {.create = create, .get_samp_rate = sample_rate,
    .get_channel_num = channel_count, .get_samp_chunksize = chunk,
    .detect = detect, .clean = clean, .destroy = destroy};
const esp_wn_iface_t *esp_wn_handle_from_name(const char *name)
{ assert(!strcmp(name, "wn9_nihaoxiaozhi")); return &mock; }
vad_handle_t vad_create(vad_mode_t mode)
{ assert(mode == VAD_MODE_3 && !live_vad); if (fail_vad) return NULL; ++live_vad; return &vad_instance; }
void vad_destroy(vad_handle_t v) { assert(v == &vad_instance && live_vad); --live_vad; }
vad_state_t vad_process(vad_handle_t v, int16_t *s, int hz, int ms)
{ assert(v == &vad_instance && hz == 16000 && ms == 30); return s[0] ? VAD_SPEECH : VAD_SILENCE; }

int main(void)
{
    assert(!xiaozhi_wake_init(-1, "ni hao xiao zhi"));
    assert(!xiaozhi_wake_init(3, "ni hao guan jia"));
    int16_t pcm[] = {32767, -32768, -32768, 32767, 123, -123, 0, 32767};
    int mp, rp;
    xiaozhi_pcm_mono(pcm, 4, &mp, &rp);
    assert(pcm[0] == 32767 && pcm[1] == -32768 && pcm[2] == 123 && pcm[3] == 0);
    assert(mp == 32768 && rp == 32768); /* No reference mixing or signed overflow. */
    int16_t silence[960] = {0};
    for (int i = 0; i < 20; ++i) {
        assert(xiaozhi_wake_init(0, "ni hao xiao zhi") && xiaozhi_wake_init(0, "ni hao xiao zhi"));
        assert(xiaozhi_wake_chunk() == 512 && !xiaozhi_wake_detect(silence));
        silence[0] = 123;
        assert(xiaozhi_wake_detect(silence) && xiaozhi_voice_active(silence, 960));
        silence[0] = 0;
        xiaozhi_wake_reset(); /* Must release, not call the crashing library clean. */
        assert(!live_models && !live_vad && !instance.alive && !xiaozhi_wake_chunk());
        xiaozhi_wake_free(); /* Disable after disconnect is idempotent. */
    }
    fail_model = true; assert(!xiaozhi_wake_init(0, "ni hao xiao zhi")); fail_model = false;
    fail_vad = true; assert(!xiaozhi_wake_init(0, "ni hao xiao zhi")); fail_vad = false;
    channels = 2; assert(!xiaozhi_wake_init(0, "ni hao xiao zhi")); channels = 1;
    rate = 24000; assert(!xiaozhi_wake_init(0, "ni hao xiao zhi")); rate = 16000;
    assert(xiaozhi_wake_init(0, "ni hao xiao zhi")); xiaozhi_wake_free();
    assert(created == destroyed && !live_models && !live_vad);
    puts("Xiaozhi wake lifecycle and microphone/reference tests passed");
}
