/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Ambient light and presence sensing with the on-board MIPI-CSI camera
 * (OV02C10 on paneljc, SC2356/SC202CS on Tab5, OV5647 on LCD-X).
 * The V4L2 capture flow (esp_video, RGB565 through the ISP, user-pointer
 * buffers) follows the approach of the BETTA-kruzio project's local camera
 * module, reduced to a duty-cycled light sensor.
 */
#include "drivers/ambient_camera.h"
#include "drivers/ambient_camera_policy.h"

#include <stddef.h>
#include <string.h>

#include "sdkconfig.h"

#if CONFIG_APP_FEATURE_AMBIENT_CAMERA

#if defined(AMBIENT_CAMERA_TEST)
#include "ambient_camera_test_platform.h"
#else
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "esp_cam_sensor_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

#include "drivers/display_init.h"
#include "settings/night_mode.h"
#endif

#define TAG_AMBIENT "ambient_cam"

#define AMBIENT_BUFS 2
#define AMBIENT_BURST_FRAMES 40        /* the ISP auto exposure needs about a second to settle */
#define AMBIENT_CACHE_LINE 64

#if defined(AMBIENT_CAMERA_TEST)
/* Board APIs are supplied by the host harness. */
#elif defined(CONFIG_APP_PANEL_VARIANT_TAB5_1280)
#include "bsp/m5stack_tab5.h"
#elif defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#else
extern i2c_master_bus_handle_t touch_paneljc_i2c_bus(void);
#endif

static ambient_camera_reading_t s_reading;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_prev_grid[AMBIENT_GRID * AMBIENT_GRID];
static bool s_have_prev;
static int s_fd = -1;
static uint8_t *s_bufs[AMBIENT_BUFS];
static size_t s_buf_size;
static uint32_t s_width;
static uint32_t s_height;
static int32_t s_base_gain = 1;
static bool s_video_initialized;
static int64_t s_motion_ms;
typedef struct {
    bool auto_brightness, presence_wake, night_wake;
    int presence_pct;
    unsigned generation;
} ambient_config_t;
static ambient_config_t s_config = {.presence_pct = 8};
static int64_t s_test_until_ms;

static ambient_config_t ambient_config(void)
{
    portENTER_CRITICAL(&s_lock);
    ambient_config_t config = s_config;
    portEXIT_CRITICAL(&s_lock);
    return config;
}

static void ambient_close(void)
{
    if (s_fd >= 0) { close(s_fd); s_fd = -1; }
    if (s_video_initialized) { (void)esp_video_deinit(); s_video_initialized = false; }
    for (int i = 0; i < AMBIENT_BUFS; i++) {
        heap_caps_free(s_bufs[i]);
        s_bufs[i] = NULL;
    }
}

static int ambient_ioctl(int cmd, void *arg)
{
    return ioctl(s_fd, cmd, arg);
}

static bool ambient_prepare_buffers(void)
{
    struct v4l2_requestbuffers rb = {
        .count = AMBIENT_BUFS,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
    };
    return ambient_ioctl(VIDIOC_REQBUFS, &rb) == 0;
}

static esp_err_t ambient_open(void)
{
#if defined(CONFIG_APP_PANEL_VARIANT_TAB5_1280)
    /* The BSP enables camera power through the I/O expander and shares its
     * initialized I2C bus with touch before it registers /dev/video0. */
    esp_err_t err = bsp_camera_start(NULL);
#else
#if defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
    /* LCD-X camera, touch and codecs share the BSP-owned GPIO7/8 bus. */
    esp_err_t bus_err = bsp_i2c_init();
    if (bus_err != ESP_OK) return bus_err;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
#else
    i2c_master_bus_handle_t bus = touch_paneljc_i2c_bus();
#endif
    esp_video_init_csi_config_t csi[] = {{
        .sccb_config = {
            .init_sccb = false,
            .i2c_handle = bus,
            .freq = 100000,
        },
        .reset_pin = -1,
        .pwdn_pin = -1,
    }};
    if (csi[0].sccb_config.i2c_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_video_init_config_t cfg = {.csi = csi};
    esp_err_t err = esp_video_init(&cfg);
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG_AMBIENT, "camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_video_initialized = true;
    s_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY | O_NONBLOCK);
    if (s_fd < 0) {
        ESP_LOGE(TAG_AMBIENT, "open %s failed", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
        return ESP_FAIL;
    }
    /* esp_video 2.3 ignores open flags and defaults DQBUF to portMAX_DELAY.
     * Its explicit timeout ioctl is required for the bounded polling below. */
    struct timeval timeout = {0};
    if (ambient_ioctl(VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        return ESP_FAIL;
    }

    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if (ambient_ioctl(VIDIOC_G_FMT, &fmt) != 0) {
        return ESP_FAIL;
    }
    s_width = fmt.fmt.pix.width;
    s_height = fmt.fmt.pix.height;
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        struct v4l2_format req = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .fmt.pix = {.width = s_width, .height = s_height, .pixelformat = V4L2_PIX_FMT_RGB565},
        };
        if (ambient_ioctl(VIDIOC_S_FMT, &req) != 0) {
            ESP_LOGE(TAG_AMBIENT, "S_FMT RGB565 failed");
            return ESP_FAIL;
        }
    }

    /* Read back the negotiated format before allocating/indexing frames. */
    if (ambient_ioctl(VIDIOC_G_FMT, &fmt) != 0 || fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 ||
        fmt.fmt.pix.width < 64 || fmt.fmt.pix.height < 64 ||
        fmt.fmt.pix.width > 2048 || fmt.fmt.pix.height > 2048 ||
        (fmt.fmt.pix.bytesperline != 0 && fmt.fmt.pix.bytesperline != fmt.fmt.pix.width * 2)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    s_width = fmt.fmt.pix.width;
    s_height = fmt.fmt.pix.height;
    s_buf_size = (size_t)s_width * s_height * 2;
    for (int i = 0; i < AMBIENT_BUFS; i++) {
        s_bufs[i] = heap_caps_aligned_calloc(AMBIENT_CACHE_LINE, 1, s_buf_size, MALLOC_CAP_SPIRAM);
        if (s_bufs[i] == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!ambient_prepare_buffers()) {
        ESP_LOGE(TAG_AMBIENT, "REQBUFS failed");
        return ESP_FAIL;
    }

    struct v4l2_query_ext_ctrl qc = {.id = V4L2_CID_GAIN};
    struct v4l2_querymenu qm = {.id = V4L2_CID_GAIN};
    if (ambient_ioctl(VIDIOC_QUERY_EXT_CTRL, &qc) == 0) {
        qm.index = (uint32_t)qc.minimum;
        if (ambient_ioctl(VIDIOC_QUERYMENU, &qm) == 0 && qm.value > 0) {
            s_base_gain = (int32_t)qm.value;
        }
    }
    ESP_LOGI(TAG_AMBIENT, "camera ready %" PRIu32 "x%" PRIu32 ", base gain %d", s_width, s_height, (int)s_base_gain);
    return ESP_OK;
}

static bool ambient_queue_buffer(int index)
{
    struct v4l2_buffer b = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
        .index = (uint32_t)index,
    };
    b.m.userptr = (uintptr_t)s_bufs[index];
    b.length = s_buf_size;
    return ambient_ioctl(VIDIOC_QBUF, &b) == 0;
}

#if !defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
static float ambient_read_gain(void)
{
    /* The gain control is a menu: the current value is an index into it. */
    struct v4l2_ext_control c = {.id = V4L2_CID_GAIN};
    struct v4l2_ext_controls cs = {.ctrl_class = V4L2_CID_USER_CLASS, .count = 1, .controls = &c};
    if (ambient_ioctl(VIDIOC_G_EXT_CTRLS, &cs) != 0) {
        return 1.0f;
    }
    struct v4l2_querymenu qm = {.id = V4L2_CID_GAIN, .index = (uint32_t)c.value};
    if (ambient_ioctl(VIDIOC_QUERYMENU, &qm) != 0 || qm.value <= 0) {
        return 1.0f;
    }
    return (float)qm.value / (float)s_base_gain;
}

static float ambient_read_exposure(void)
{
    struct v4l2_ext_control c = {.id = V4L2_CID_EXPOSURE};
    struct v4l2_ext_controls cs = {.ctrl_class = V4L2_CID_CAMERA_CLASS, .count = 1, .controls = &c};
    if (ambient_ioctl(VIDIOC_G_EXT_CTRLS, &cs) != 0) {
        return 1.0f;
    }
    return (float)c.value;
}

#endif

static bool ambient_read_light_controls(float *exposure, float *gain)
{
#if defined(CONFIG_APP_PANEL_VARIANT_10INCH_1280)
    /* OV5647 uses on-sensor AEC/AGC. Its exposure control sets an AE target,
     * not shutter time. Use actual read-only register statistics instead. */
    esp_cam_sensor_stats_t stats = {0};
    struct v4l2_ext_control c = {.id = V4L2_CID_CAMERA_STATS, .size = sizeof(stats), .p_u8 = (uint8_t *)&stats};
    struct v4l2_ext_controls cs = {.ctrl_class = V4L2_CID_CAMERA_CLASS, .count = 1, .controls = &c};
    if (ambient_ioctl(VIDIOC_G_EXT_CTRLS, &cs) != 0 || stats.aec_exp == 0 || stats.agc_gain < 1.0f) return false;
    *exposure = (float)stats.aec_exp;
    *gain = stats.agc_gain;
#else
    *exposure = ambient_read_exposure();
    *gain = ambient_read_gain();
#endif
    return true;
}

static inline uint8_t ambient_luma(uint16_t px)
{
    const uint32_t r = (px >> 11) & 0x1F;
    const uint32_t g = (px >> 5) & 0x3F;
    const uint32_t b = px & 0x1F;
    /* Rec.601 weights on 5/6/5-bit channels scaled to 0..255. */
    return (uint8_t)((r * 8 * 77 + g * 4 * 150 + b * 8 * 29) >> 8);
}

/* Reduce the frame to a coarse luma grid (each cell samples a 4x4 pixel patch). */
static void ambient_frame_to_grid(const uint8_t *frame, uint8_t *grid)
{
    const uint16_t *px = (const uint16_t *)frame;
    for (int gy = 0; gy < AMBIENT_GRID; gy++) {
        for (int gx = 0; gx < AMBIENT_GRID; gx++) {
            const uint32_t x0 = (uint32_t)((gx * 2 + 1) * s_width / (AMBIENT_GRID * 2)) - 2;
            const uint32_t y0 = (uint32_t)((gy * 2 + 1) * s_height / (AMBIENT_GRID * 2)) - 2;
            uint32_t sum = 0;
            for (uint32_t y = 0; y < 4; y++) {
                for (uint32_t x = 0; x < 4; x++) {
                    sum += ambient_luma(px[(y0 + y) * s_width + x0 + x]);
                }
            }
            grid[gy * AMBIENT_GRID + gx] = (uint8_t)(sum / 16);
        }
    }
}

static int64_t s_last_light_ms; /* sampling task only */

/* Frame index whose grid is used for movement: the ISP auto exposure restarts
 * the same way on every stream start, so this frame always looks alike. */
#define AMBIENT_MOTION_FRAME 7

#define AMBIENT_LIGHT_INTERVAL_MS 20000

static int64_t ambient_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* The first frame after a pause establishes a baseline; it cannot wake. */
static bool ambient_motion(const uint8_t *grid, int *changed_pct, int *level)
{
    bool valid = s_have_prev;
    *changed_pct = valid ? ambient_grid_motion(grid, s_prev_grid, level) : 0;
    if (!valid) *level = 0;
    memcpy(s_prev_grid, grid, sizeof(s_prev_grid));
    s_have_prev = true;
    return valid;
}

static bool ambient_sample(int frames, bool want_light, int64_t deadline_ms)
{
    if (ambient_now_ms() >= deadline_ms) return false;
    for (int i = 0; i < AMBIENT_BUFS; i++) {
        if (!ambient_queue_buffer(i)) {
            ESP_LOGW(TAG_AMBIENT, "QBUF %d failed", i);
            (void)ambient_prepare_buffers();
            return false;
        }
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ambient_ioctl(VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGW(TAG_AMBIENT, "STREAMON failed");
        /* esp_video retains queued buffers after a failed start. STREAMOFF
         * cannot reset a stream that never started; REQBUFS can. */
        (void)ambient_prepare_buffers();
        return false;
    }

    uint8_t motion_grid[AMBIENT_GRID * AMBIENT_GRID];
    bool have_motion_grid = false;
    int last = -1;
    int captured = 0;
    for (int i = 0; i < frames; i++) {
        struct v4l2_buffer b = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_USERPTR,
        };
        /* Non-blocking descriptor: poll for up to one second per frame. */
        int waited_ms = 0;
        while (ambient_ioctl(VIDIOC_DQBUF, &b) != 0 && waited_ms < 1000 && ambient_now_ms() < deadline_ms) {
            vTaskDelay(pdMS_TO_TICKS(5));
            waited_ms += 5;
        }
        if (waited_ms >= 1000 || ambient_now_ms() >= deadline_ms) {
            ESP_LOGW(TAG_AMBIENT, "frame %d timed out", i);
            break;
        }
        if (b.index >= AMBIENT_BUFS || b.bytesused < s_buf_size) break;
        last = (int)b.index;
        captured++;
        if (i == AMBIENT_MOTION_FRAME) {
            ambient_frame_to_grid(s_bufs[last], motion_grid);
            have_motion_grid = true;
        }
        if (i < frames - 1 && !ambient_queue_buffer(last)) {
            break;
        }
    }

    if (ambient_ioctl(VIDIOC_STREAMOFF, &type) != 0) return false;
    float exposure = 0, gain = 0;
    const bool controls_valid = !want_light || ambient_read_light_controls(&exposure, &gain);
    if (captured != frames || last < 0 || !have_motion_grid || !controls_valid || ambient_now_ms() >= deadline_ms) {
        ESP_LOGW(TAG_AMBIENT, "no usable frame");
        return false;
    }

    ambient_camera_reading_t r;
    portENTER_CRITICAL(&s_lock);
    r = s_reading;
    portEXIT_CRITICAL(&s_lock);

    int changed_pct = 0;
    int level = 0;
    const bool motion_valid = ambient_motion(motion_grid, &changed_pct, &level);
    r.ready = true;
    r.failed = false;
    r.motion_valid = motion_valid;
    r.motion_pct = motion_valid ? changed_pct : 0;
    r.motion_level = motion_valid ? level : 0;

    if (want_light) {
        uint8_t grid[AMBIENT_GRID * AMBIENT_GRID];
        ambient_frame_to_grid(s_bufs[last], grid);
        uint32_t sum = 0;
        for (int i = 0; i < AMBIENT_GRID * AMBIENT_GRID; i++) {
            sum += grid[i];
        }
        r.luma = (float)sum / (AMBIENT_GRID * AMBIENT_GRID);
        r.exposure = exposure;
        r.gain = gain;
        /* Relative scene brightness: linearised pixel value divided by what
         * the sensor needed (exposure * gain) to get there. */
        const float lin = powf(fmaxf(r.luma, 1.0f) / 255.0f, 2.2f);
        r.light_index = log10f(lin / fmaxf(exposure * gain, 1.0f));
        r.valid = true;
        s_last_light_ms = ambient_now_ms();
        ESP_LOGI(TAG_AMBIENT, "luma %.1f exposure %.0f gain %.2f light %.2f -> %d%%", (double)r.luma,
            (double)exposure, (double)gain, (double)r.light_index, ambient_light_percent(r.light_index));
        if (ambient_config().auto_brightness) {
            display_set_ambient_limit_percent(ambient_light_percent(r.light_index));
        }
    }
    portENTER_CRITICAL(&s_lock);
    s_reading = r;
    s_motion_ms = ambient_now_ms();
    portEXIT_CRITICAL(&s_lock);

    /* Someone moved in front of the panel while it is dimmed: wake it up. */
    ambient_config_t config = ambient_config();
    if (motion_valid && r.motion_pct >= config.presence_pct &&
        ambient_motion_needed(false, config.presence_wake, display_is_dimmed(), night_mode_active(), config.night_wake)) {
        ESP_LOGI(TAG_AMBIENT, "presence: %d%% of the cells changed, waking the display", r.motion_pct);
        display_note_activity();
    }
    return true;
}

static void ambient_reset_motion(bool failed)
{
    s_have_prev = false;
    portENTER_CRITICAL(&s_lock);
    s_reading.motion_valid = false;
    s_reading.motion_pct = 0;
    s_reading.failed = failed;
    if (failed) s_reading.valid = false;
    portEXIT_CRITICAL(&s_lock);
}

static void ambient_task(void *arg)
{
    (void)arg;
    /* Let boot finish (Wi-Fi, LVGL) before touching the camera. */
    vTaskDelay(pdMS_TO_TICKS(20000));
    bool opened = false;
    unsigned generation = 0;
    for (;;) {
        ambient_config_t config = ambient_config();
        bool test_active = ambient_camera_test_active(NULL);
        if (config.generation != generation) {
            generation = config.generation;
            s_last_light_ms = 0;
            ambient_reset_motion(false);
        }
        if (!config.auto_brightness && !config.presence_wake && !test_active) {
            ambient_reset_motion(false);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (!opened) {
            if (ambient_open() != ESP_OK) {
                ambient_close();
                ambient_reset_motion(true);
                ESP_LOGE(TAG_AMBIENT, "camera unavailable, ambient sensing disabled until reboot");
                vTaskDelete(NULL);
                return;
            }
            opened = true;
            portENTER_CRITICAL(&s_lock);
            s_reading.ready = true;
            portEXIT_CRITICAL(&s_lock);
        }
        const bool need_light = config.auto_brightness &&
            (s_last_light_ms == 0 || ambient_now_ms() - s_last_light_ms >= AMBIENT_LIGHT_INTERVAL_MS);
        const bool need_motion = ambient_motion_needed(test_active, config.presence_wake,
            display_is_dimmed(), night_mode_active(), config.night_wake);
        if (need_light || need_motion) {
            int64_t deadline_ms = ambient_now_ms() + 2000;
            if (test_active && !need_light && !ambient_motion_needed(false, config.presence_wake,
                    display_is_dimmed(), night_mode_active(), config.night_wake)) {
                portENTER_CRITICAL(&s_lock);
                if (s_test_until_ms < deadline_ms) deadline_ms = s_test_until_ms;
                portEXIT_CRITICAL(&s_lock);
            }
            if (!ambient_sample(need_light ? AMBIENT_BURST_FRAMES : AMBIENT_MOTION_FRAME + 1,
                                need_light, deadline_ms)) {
                ambient_reset_motion(true);
            }
        } else {
            ambient_reset_motion(false);
        }
        vTaskDelay(pdMS_TO_TICKS(need_motion ? 1000 : 2000));
    }
}

void ambient_camera_configure(bool auto_brightness, bool presence_wake, int sensitivity, bool night_wake)
{
    portENTER_CRITICAL(&s_lock);
    s_config.auto_brightness = auto_brightness;
    s_config.presence_wake = presence_wake;
    s_config.night_wake = night_wake;
    s_config.presence_pct = ambient_presence_threshold(sensitivity);
    s_config.generation++;
    portEXIT_CRITICAL(&s_lock);
    if (!auto_brightness) display_set_ambient_limit_percent(100);
}

void ambient_camera_start(void)
{
    /* Called after display_init on the same boot task. IDF 6.1 puts CSI and
     * DSI DMA interrupts in one private group, bound to the display's core.
     * Every burst must allocate/free CSI on that core, even after idle. */
    if (xTaskCreatePinnedToCore(ambient_task, "ambient_cam", 6 * 1024, NULL, 3, NULL, xPortGetCoreID()) != pdPASS) {
        ambient_reset_motion(true);
        ESP_LOGE(TAG_AMBIENT, "could not create camera task");
    }
}

bool ambient_camera_get(ambient_camera_reading_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    *out = s_reading;
    if (ambient_now_ms() - s_motion_ms > 4000) out->motion_valid = false;
    portEXIT_CRITICAL(&s_lock);
    return out->valid;
}

bool ambient_camera_available(void)
{
    return true;
}

int ambient_camera_get_presence_threshold_pct(void)
{
    return ambient_config().presence_pct;
}

void ambient_camera_start_test(uint32_t duration_ms)
{
    if (duration_ms > AMBIENT_TEST_MAX_MS) duration_ms = AMBIENT_TEST_MAX_MS;
    portENTER_CRITICAL(&s_lock);
    /* Repeated requests and sensitivity edits must not extend a running test. */
    if (!ambient_test_remaining_s(s_test_until_ms, ambient_now_ms())) {
        s_test_until_ms = ambient_now_ms() + duration_ms;
        s_reading.motion_valid = false;
        s_config.generation++;
    }
    portEXIT_CRITICAL(&s_lock);
}

bool ambient_camera_test_active(int *out_remaining_s)
{
    portENTER_CRITICAL(&s_lock);
    int seconds = ambient_test_remaining_s(s_test_until_ms, ambient_now_ms());
    portEXIT_CRITICAL(&s_lock);
    if (out_remaining_s != NULL) *out_remaining_s = seconds;
    return seconds > 0;
}

#else /* !CONFIG_APP_FEATURE_AMBIENT_CAMERA */

void ambient_camera_start(void) {}

void ambient_camera_configure(bool auto_brightness, bool presence_wake, int sensitivity, bool night_wake)
{
    (void)auto_brightness;
    (void)presence_wake;
    (void)sensitivity;
    (void)night_wake;
}

bool ambient_camera_get(ambient_camera_reading_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return false;
}

bool ambient_camera_available(void)
{
    return false;
}

int ambient_camera_get_presence_threshold_pct(void)
{
    return 0;
}

void ambient_camera_start_test(uint32_t duration_ms)
{
    (void)duration_ms;
}

bool ambient_camera_test_active(int *out_remaining_s)
{
    if (out_remaining_s != NULL) {
        *out_remaining_s = 0;
    }
    return false;
}

#endif
