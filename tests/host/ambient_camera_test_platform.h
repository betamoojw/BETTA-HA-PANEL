/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Small deterministic camera/RTOS boundary for the real ambient worker.
 */
#pragma once
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>

typedef int esp_err_t;
typedef void *i2c_master_bus_handle_t;
typedef int portMUX_TYPE;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define ESP_ERR_NOT_SUPPORTED -3
#define ESP_ERR_NO_MEM -4
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
#define MALLOC_CAP_SPIRAM 1
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define O_RDONLY 0
#define O_NONBLOCK 1
#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME "/dev/video0"
#define open camera_mock_open
#define close camera_mock_close
#define ioctl camera_mock_ioctl

enum { VIDIOC_S_DQBUF_TIMEOUT = 1, VIDIOC_G_FMT, VIDIOC_S_FMT, VIDIOC_REQBUFS,
       VIDIOC_QUERY_EXT_CTRL, VIDIOC_QUERYMENU, VIDIOC_QBUF, VIDIOC_G_EXT_CTRLS,
       VIDIOC_STREAMON, VIDIOC_DQBUF, VIDIOC_STREAMOFF };
enum { V4L2_BUF_TYPE_VIDEO_CAPTURE = 1, V4L2_MEMORY_USERPTR, V4L2_PIX_FMT_RGB565,
       V4L2_CID_GAIN, V4L2_CID_CAMERA_STATS, V4L2_CID_CAMERA_CLASS };
struct v4l2_format {
    int type;
    struct { struct { uint32_t width, height, pixelformat, bytesperline; } pix; } fmt;
};
struct v4l2_requestbuffers { int count, type, memory; };
struct v4l2_query_ext_ctrl { int id; int64_t minimum; };
struct v4l2_querymenu { int id; uint32_t index; int64_t value; };
struct v4l2_buffer {
    int type, memory;
    uint32_t index;
    union { uintptr_t userptr; } m;
    size_t length, bytesused;
};
struct v4l2_ext_control { int id; size_t size; uint8_t *p_u8; };
struct v4l2_ext_controls { int ctrl_class, count; struct v4l2_ext_control *controls; };
typedef struct { uint32_t flags, seq, aec_exp; float agc_gain; } esp_cam_sensor_stats_t;
typedef struct {
    struct { bool init_sccb; i2c_master_bus_handle_t i2c_handle; int freq; } sccb_config;
    int reset_pin, pwdn_pin;
} esp_video_init_csi_config_t;
typedef struct { esp_video_init_csi_config_t *csi; } esp_video_init_config_t;

esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
esp_err_t esp_video_init(const esp_video_init_config_t *);
esp_err_t esp_video_deinit(void);
void *heap_caps_aligned_calloc(size_t, size_t, size_t, int);
void heap_caps_free(void *);
int camera_mock_open(const char *, int);
int camera_mock_close(int);
int camera_mock_ioctl(int, int, void *);
int64_t esp_timer_get_time(void);
void vTaskDelay(unsigned);
void vTaskDelete(void *);
int xPortGetCoreID(void);
int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned, void *, unsigned, void *, int);
void display_set_ambient_limit_percent(int);
void display_note_activity(void);
bool display_is_dimmed(void);
bool night_mode_active(void);
