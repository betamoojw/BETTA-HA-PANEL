/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Exercise the actual camera driver with deterministic frames and failures.
 */
#include <assert.h>
#include <stdio.h>
#include "../../main/drivers/ambient_camera.c"

static int64_t now_ms = 5000000000;
static unsigned allocated, init_calls, deinit_calls, frame_no, wakes;
static int alloc_fail_at = -1, format_stride = 128, fail_frame = -1;
static int ambient_limit = 100;
static bool timeout_set, controls_fail, task_fail, dimmed, night, changed;
static bool start_fail;
static unsigned queue_mask;

esp_err_t bsp_i2c_init(void) { return ESP_OK; }
i2c_master_bus_handle_t bsp_i2c_get_handle(void) { return (void *)1; }
esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    assert(!config->csi->sccb_config.init_sccb);
    assert(config->csi->sccb_config.i2c_handle == (void *)1);
    assert(config->csi->reset_pin == -1 && config->csi->pwdn_pin == -1);
    init_calls++;
    return ESP_OK;
}
esp_err_t esp_video_deinit(void) { deinit_calls++; return ESP_OK; }
void *heap_caps_aligned_calloc(size_t align, size_t n, size_t size, int caps)
{
    assert(align == 64 && caps == MALLOC_CAP_SPIRAM);
    if ((int)allocated == alloc_fail_at) return NULL;
    void *p = calloc(n, size);
    if (p) allocated++;
    return p;
}
void heap_caps_free(void *p) { if (p) { allocated--; free(p); } }
int camera_mock_open(const char *path, int flags) { (void)path; (void)flags; return 4; }
int camera_mock_close(int fd) { assert(fd == 4); return 0; }
int camera_mock_ioctl(int fd, int cmd, void *arg)
{
    assert(fd == 4);
    switch (cmd) {
    case VIDIOC_REQBUFS: queue_mask = 0; return 0;
    case VIDIOC_QBUF: {
        struct v4l2_buffer *b = arg;
        if (queue_mask & (1u << b->index)) return -1;
        queue_mask |= 1u << b->index;
        return 0;
    }
    case VIDIOC_S_DQBUF_TIMEOUT: {
        struct timeval *t = arg;
        assert(t->tv_sec == 0 && t->tv_usec == 0);
        timeout_set = true;
        return 0;
    }
    case VIDIOC_G_FMT: {
        struct v4l2_format *f = arg;
        f->fmt.pix.width = 64; f->fmt.pix.height = 64;
        f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
        f->fmt.pix.bytesperline = format_stride;
        return 0;
    }
    case VIDIOC_QUERY_EXT_CTRL: return -1; /* OV5647 has no gain-menu control. */
    case VIDIOC_STREAMON: frame_no = 0; return start_fail ? -1 : 0;
    case VIDIOC_STREAMOFF: queue_mask = 0; return 0;
    case VIDIOC_DQBUF: {
        assert(timeout_set); /* O_NONBLOCK alone does not bound esp_video. */
        if ((int)frame_no == fail_frame) return -1;
        struct v4l2_buffer *b = arg;
        b->index = frame_no++ % 2;
        queue_mask &= ~(1u << b->index);
        b->bytesused = s_buf_size;
        uint16_t *pixels = (uint16_t *)s_bufs[b->index];
        for (unsigned i = 0; i < s_width * s_height; i++)
            pixels[i] = changed && i < s_width * s_height / 4 ? 0xffff : 0x4208;
        now_ms += 20;
        return 0;
    }
    case VIDIOC_G_EXT_CTRLS: {
        if (controls_fail) return -1;
        struct v4l2_ext_controls *cs = arg;
        assert(cs->controls->id == V4L2_CID_CAMERA_STATS);
        esp_cam_sensor_stats_t *stats = (void *)cs->controls->p_u8;
        stats->aec_exp = 984; stats->agc_gain = 15.5f;
        return 0;
    }
    default: return 0;
    }
}
int64_t esp_timer_get_time(void) { return now_ms * 1000; }
void vTaskDelay(unsigned ms) { now_ms += ms; }
void vTaskDelete(void *task) { (void)task; }
int xPortGetCoreID(void) { return 1; }
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *task, int core)
{
    assert(core == xPortGetCoreID());
    (void)fn; (void)name; (void)stack; (void)arg; (void)priority; (void)task;
    return task_fail ? 0 : pdPASS;
}
void display_set_ambient_limit_percent(int percent) { ambient_limit = percent; }
void display_note_activity(void) { wakes++; }
bool display_is_dimmed(void) { return dimmed; }
bool night_mode_active(void) { return night; }

int main(void)
{
    assert(ambient_camera_available());
    ambient_camera_configure(false, false, 5, false);
    ambient_camera_start_test(UINT32_MAX);
    int seconds;
    assert(ambient_camera_test_active(&seconds) && seconds == 30);
    now_ms += 10000;
    ambient_camera_start_test(30000);
    ambient_camera_configure(false, false, 9, true);
    assert(ambient_camera_test_active(&seconds) && seconds == 20);
    now_ms += 20000;
    assert(!ambient_camera_test_active(&seconds) && seconds == 0);

    alloc_fail_at = 1;
    assert(ambient_open() == ESP_ERR_NO_MEM);
    ambient_close();
    assert(allocated == 0 && init_calls == deinit_calls);
    alloc_fail_at = -1;
    format_stride = 256;
    assert(ambient_open() == ESP_ERR_NOT_SUPPORTED);
    ambient_close();
    format_stride = 128;
    assert(ambient_open() == ESP_OK && allocated == 2);
    /* An expired test must not even start a stream. */
    frame_no = 123;
    assert(!ambient_sample(8, false, now_ms));
    assert(frame_no == 123);
    start_fail = true;
    assert(!ambient_sample(8, false, now_ms + 2000));
    assert(queue_mask == 0); /* Failed starts must allow a later retry. */
    start_fail = false;
    assert(ambient_sample(40, true, now_ms + 2000));
    ambient_camera_reading_t reading;
    assert(ambient_camera_get(&reading));
    assert(reading.exposure == 984 && reading.gain == 15.5f && !reading.motion_valid);
    assert(ambient_limit == 100); /* A test must not enable auto brightness. */

    ambient_camera_configure(true, true, 5, false);
    dimmed = true; night = true; changed = true;
    assert(ambient_sample(8, false, now_ms + 2000) && wakes == 0);
    ambient_camera_configure(true, true, 5, true);
    changed = false;
    assert(ambient_sample(8, false, now_ms + 2000) && wakes == 1);
    assert(ambient_sample(40, true, now_ms + 2000) && ambient_limit < 100);
    ambient_camera_configure(false, false, 5, false);
    assert(ambient_limit == 100);

    /* A partial light burst and a failed control read cannot publish a reading. */
    fail_frame = 10;
    int64_t before = now_ms;
    assert(!ambient_sample(40, true, now_ms + 2000));
    assert(now_ms - before <= 2000);
    fail_frame = -1; controls_fail = true;
    assert(!ambient_sample(40, true, now_ms + 2000));
    ambient_reset_motion(true);
    assert(!ambient_camera_get(&reading) && reading.failed && !reading.motion_valid);
    controls_fail = false;
    ambient_close();
    assert(allocated == 0 && init_calls == deinit_calls);
    task_fail = true;
    ambient_camera_start();
    assert(!ambient_camera_get(&reading) && reading.failed);
    puts("ambient runtime: shared bus, timeout, expiry, memory cleanup, sensing and failures passed");
    return 0;
}
