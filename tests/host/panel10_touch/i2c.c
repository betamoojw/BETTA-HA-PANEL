/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * HAL/RTOS fixture only. The SDK send loop, panel IO and BSP are inserted verbatim.
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

typedef int esp_err_t;
typedef uint32_t TickType_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_INVALID_ARG 3
#define ESP_ERR_NO_MEM 4
#define ESP_ERR_NOT_FOUND 5
#define pdTRUE 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define ESP_LOGE(tag,...) ((void)(tag))
#define ESP_LOGD(tag,...) ((void)(tag))
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGW(tag,...) ((void)(tag))
#define ESP_ERROR_CHECK(c) assert((c) == ESP_OK)
#define ESP_GOTO_ON_FALSE(c,e,l,...) do { if (!(c)) { ret=(e); goto l; } } while (0)
#define ESP_GOTO_ON_ERROR(c,l,...) do { ret=(c); if (ret != ESP_OK) goto l; } while (0)
#define ESP_RETURN_ON_ERROR(c,...) do { esp_err_t result=(c); if (result != ESP_OK) return result; } while (0)
#define ESP_RETURN_ON_FALSE(c,e,...) do { if (!(c)) return (e); } while (0)
#define BSP_ERROR_CHECK_RETURN_ERR(c) ESP_RETURN_ON_ERROR(c)
#define ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE(x)
#define ESP_COMPILER_DIAGNOSTIC_POP(x)
#define __containerof(p,t,m) ((t *)((char *)(p) - offsetof(t,m)))
#define portENTER_CRITICAL_SAFE(x) ((void)(x))
#define portEXIT_CRITICAL_SAFE(x) ((void)(x))

static unsigned allocations, live_devices, resets, recoveries, takes, gives, bus_takes, bus_gives, queue_resets, busy_polls;
static bool bus_available=true, bus_held;
static bool allocation_failure;
static int bus_tag, cmd_tag;
static TickType_t tick;
static int scenario, last_timeout, last_address, probe_address = 0x5d;
static bool queue_available = true, semaphore_available = true;
static unsigned clear_after;
static void *counted_calloc(size_t n, size_t s) { if(allocation_failure) { allocation_failure=false;return NULL; } void *p=calloc(n,s); if(p) ++allocations; return p; }
static void counted_free(void *p) { if(p) { assert(allocations); --allocations; free(p); } }
#define calloc counted_calloc
#define free counted_free
enum { I2C_EVENT_ALIVE, I2C_EVENT_DONE, I2C_EVENT_NACK };
enum { I2C_STATUS_ACK_ERROR, I2C_STATUS_TIMEOUT, I2C_STATUS_DONE };
enum { I2C_LL_CMD_WRITE, I2C_LL_CMD_READ, I2C_LL_CMD_STOP };
typedef int i2c_master_event_t;
typedef struct { int op_code; } i2c_ll_hw_cmd_t;
typedef struct { i2c_ll_hw_cmd_t hw_cmd; } i2c_operation_t;
typedef struct { void *dev; } i2c_hal_context_t;
typedef struct { i2c_hal_context_t hal; int spinlock; } bus_base_t;
typedef struct { unsigned device_address, cmd_count; i2c_operation_t *ops; } i2c_transaction_t;
typedef struct {
    bus_base_t *base;
    i2c_transaction_t i2c_trans;
    i2c_operation_t i2c_ops[3];
    bool trans_done,addr_10bits_bus,sent_all,trans_finish,queue_trans,ack_check_disable;
    void *bus_lock_mux;
    unsigned trans_idx, cmd_idx;
    void *event_queue, *cmd_semphr;
    int event;
    atomic_int status;
    bool bypass_nack_log;
} bus_t;
typedef bus_t *i2c_master_bus_handle_t;
typedef struct { bus_t *master_bus; bool addr_10bits,ack_check_disable; unsigned device_address; } device_t;
typedef device_t *i2c_master_dev_handle_t;
typedef struct { unsigned device_address, scl_speed_hz; } i2c_device_config_t;
typedef struct { uint8_t *write_buffer; size_t buffer_size; } i2c_master_transmit_multi_buffer_info_t;
static bus_base_t base;
static bus_t bus = {.base=&base,.bus_lock_mux=&bus_tag,.cmd_semphr=&cmd_tag};
static device_t device={.master_bus=&bus};
static i2c_master_bus_handle_t i2c_handle = &bus;
static void xQueueReset(void *q) { (void)q; ++queue_resets; }
static int xSemaphoreTake(void *s, TickType_t timeout) {
    assert(timeout <= 50);
    if(s == &bus_tag) { ++bus_takes; if(!bus_available) return 0; assert(!bus_held);bus_held=true;return pdTRUE; }
    assert(s == &cmd_tag); ++takes; return semaphore_available;
}
static void xSemaphoreGive(void *s) {
    if(s == &bus_tag) { assert(bus_held && gives == 1);bus_held=false;++bus_gives; } else { assert(s == &cmd_tag);++gives; }
}
static int xQueueReceive(void *q, i2c_master_event_t *e, TickType_t timeout) {
    (void)q; assert(timeout <= 50); *e=scenario ? I2C_EVENT_NACK : I2C_EVENT_DONE; return queue_available;
}
static TickType_t xTaskGetTickCount(void) { return tick++; }
static bool i2c_ll_is_bus_busy(void *dev) { (void)dev; assert(++busy_polls < 200); return scenario == 2 || busy_polls < clear_after; }
static void s_i2c_hw_fsm_reset(bus_t *b, bool hard) { (void)b; if(hard) ++resets; else ++recoveries; }
static void i2c_ll_master_write_cmd_reg(void *d, i2c_ll_hw_cmd_t c, int i) { (void)d; assert(c.op_code == I2C_LL_CMD_STOP && i == 0); }
static void i2c_hal_master_trans_start(i2c_hal_context_t *h) { (void)h; }
static void s_i2c_write_command(bus_t *b, i2c_operation_t *o, uint8_t *f, uint8_t *a, void *x) { (void)o;(void)f;(void)a;(void)x;b->i2c_trans.cmd_count=0; }
static void s_i2c_read_command(bus_t *b, i2c_operation_t *o, uint8_t *f, void *x) { (void)o;(void)f;(void)x;b->i2c_trans.cmd_count=0; }
static void s_i2c_start_end_command(bus_t *b, i2c_operation_t *o, uint8_t *a, void *x) { (void)o;(void)a;(void)x;b->i2c_trans.cmd_count=0; }
static void s_i2c_err_log_print(int e, bool quiet) { (void)e;(void)quiet; }
static const char *TAG;

/* SDK_SEND_COMMANDS */

static esp_err_t s_i2c_transaction_start(i2c_master_dev_handle_t dev, int timeout) {
    atomic_store(&bus.status, scenario ? I2C_STATUS_ACK_ERROR : I2C_STATUS_DONE);
    assert(dev == &device);
    s_i2c_send_commands(&bus, pdMS_TO_TICKS(timeout));
    if (atomic_load(&bus.status) == I2C_STATUS_TIMEOUT) return ESP_ERR_TIMEOUT;
    return scenario ? ESP_FAIL : ESP_OK;
}
/* SDK_SYNC_TRANSACTION */
static esp_err_t transfer(int timeout) {
    last_timeout=timeout; assert(timeout == 50);
    i2c_operation_t operation={.hw_cmd.op_code=I2C_LL_CMD_WRITE};
    busy_polls=0; tick=0; takes=gives=bus_takes=bus_gives=0;
    esp_err_t result=s_i2c_synchronous_transaction(&device,&operation,1,timeout);
    assert(!bus_held && bus_takes == 1);
    if(bus_available) assert(gives == 1 && takes == 1 && bus_gives == 1);
    else assert(!gives && !takes && !bus_gives);
    return result;
}
static esp_err_t i2c_master_transmit_receive(void *d, const void *w,size_t wn,void *r,size_t rn,int t) { (void)d;(void)w;(void)wn;(void)r;(void)rn;return transfer(t); }
static esp_err_t i2c_master_receive(void *d,void *r,size_t n,int t) { (void)d;(void)r;(void)n;return transfer(t); }
static esp_err_t i2c_master_multi_buffer_transmit(void *d,const void *b,size_t n,int t) { (void)d;(void)b;(void)n;return transfer(t); }
static int add_failure;
static esp_err_t i2c_master_bus_add_device(bus_t *b,const i2c_device_config_t *c,i2c_master_dev_handle_t *h) { (void)b; if(add_failure)return ESP_FAIL; ++live_devices;last_address=c->device_address;device.device_address=c->device_address;*h=&device;return ESP_OK; }
static esp_err_t i2c_master_bus_rm_device(void *h) { assert(h && live_devices);--live_devices;return ESP_OK; }
typedef struct esp_lcd_panel_io_t esp_lcd_panel_io_t;
typedef bool (*esp_lcd_panel_io_color_trans_done_cb_t)(esp_lcd_panel_io_t *,void *,void *);
typedef struct { esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done; } esp_lcd_panel_io_callbacks_t;
struct esp_lcd_panel_io_t {
    esp_err_t (*del)(esp_lcd_panel_io_t *);
    esp_err_t (*rx_param)(esp_lcd_panel_io_t *,int,void *,size_t);
    esp_err_t (*tx_param)(esp_lcd_panel_io_t *,int,const void *,size_t);
    esp_err_t (*tx_color)(esp_lcd_panel_io_t *,int,const void *,size_t);
    esp_err_t (*register_event_callbacks)(esp_lcd_panel_io_t *,const esp_lcd_panel_io_callbacks_t *,void *);
};
typedef esp_lcd_panel_io_t *esp_lcd_panel_io_handle_t;
typedef struct {
    unsigned dev_addr,scl_speed_hz; size_t control_phase_bytes;
    uint8_t dc_bit_offset; int lcd_cmd_bits,lcd_param_bits;
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done; void *user_ctx;
    struct { unsigned dc_low_on_data:1, disable_control_phase:1; } flags;
    int transaction_timeout_ms;
} esp_lcd_panel_io_i2c_config_t;
/* SDK_PANEL_IO */

typedef struct { esp_lcd_panel_io_handle_t io; } touch_t;
typedef touch_t *esp_lcd_touch_handle_t;
typedef struct { struct { bool swap_xy,mirror_x,mirror_y; } touch_flags; } bsp_display_cfg_t;
typedef struct {
    int x_max,y_max,rst_gpio_num,int_gpio_num;
    struct { int reset,interrupt; } levels;
    struct { bool swap_xy,mirror_x,mirror_y; } flags;
} esp_lcd_touch_config_t;
#define BSP_LCD_H_RES 1280
#define BSP_LCD_V_RES 800
#define BSP_LCD_TOUCH_RST -1
#define BSP_LCD_TOUCH_INT -1
#define CONFIG_BSP_I2C_CLK_SPEED_HZ 400000
#define ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS 0x5d
#define ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP 0x14
#define ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG() ((esp_lcd_panel_io_i2c_config_t){.dev_addr=0x5d,.control_phase_bytes=1,.lcd_cmd_bits=16,.lcd_param_bits=8,.flags.disable_control_phase=1})
static int ctor_failure;
static int init_failure;
static esp_err_t bsp_i2c_init(void) { return init_failure ? ESP_FAIL : ESP_OK; }
static esp_err_t bsp_i2c_device_probe(uint8_t a) { return a == probe_address ? ESP_OK : ESP_FAIL; }
static esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t h) { return h->del(h); }
static esp_err_t esp_lcd_touch_new_i2c_gt911(esp_lcd_panel_io_handle_t io,const esp_lcd_touch_config_t *c,esp_lcd_touch_handle_t *out) {
    assert(c->rst_gpio_num == -1 && c->int_gpio_num == -1);
    if(ctor_failure) return ESP_FAIL;
    *out=calloc(1,sizeof(touch_t));assert(*out);(*out)->io=io;return ESP_OK;
}
/* BSP_TOUCH_NEW */

static void destroy(esp_lcd_touch_handle_t tp) { assert(tp);esp_lcd_panel_io_del(tp->io);free(tp); }
int main(void) {
    bsp_display_cfg_t cfg={0}; esp_lcd_touch_handle_t tp=NULL;
    assert(bsp_touch_new(&cfg,&tp) == ESP_OK && tp && last_address == 0x5d);
    assert(((lcd_panel_io_i2c_t *)tp->io)->transaction_timeout_ms == 50);
    uint8_t data=0;
    scenario=0;
    assert(tp->io->rx_param(tp->io,0x814e,&data,1) == ESP_OK && last_timeout == 50);
    scenario=1;clear_after=3;
    assert(tp->io->rx_param(tp->io,0x814e,&data,1) == ESP_FAIL && resets == 0);
    scenario=2;
    assert(tp->io->tx_param(tp->io,0x814e,&data,1) == ESP_ERR_TIMEOUT && resets == 1);
    assert(tick >= 50 && tick <= 53);
    scenario=0;queue_available=false;
    assert(tp->io->rx_param(tp->io,-1,&data,1) == ESP_ERR_TIMEOUT);
    assert(bus.cmd_idx == 0 && bus.trans_idx == 0);
    queue_available=true;bus_available=false;
    assert(tp->io->rx_param(tp->io,0x814e,&data,1) == ESP_ERR_TIMEOUT);
    bus_available=true;semaphore_available=false;
    assert(tp->io->rx_param(tp->io,0x814e,&data,1) == ESP_ERR_TIMEOUT);
    assert(bus.cmd_idx == 0 && bus.trans_idx == 0);
    semaphore_available=true;
    assert(tp->io->rx_param(tp->io,0x814e,&data,1) == ESP_OK);
    destroy(tp);assert(!allocations && !live_devices);
    probe_address=0x14;tp=NULL;
    assert(bsp_touch_new(&cfg,&tp) == ESP_OK && last_address == 0x14);destroy(tp);
    ctor_failure=1;tp=(void *)1;
    assert(bsp_touch_new(&cfg,&tp) == ESP_FAIL && tp == NULL);
    assert(!allocations && !live_devices);
    ctor_failure=0;add_failure=1;tp=(void *)1;
    assert(bsp_touch_new(&cfg,&tp) == ESP_FAIL && tp == NULL);
    assert(!allocations && !live_devices);
    add_failure=0;probe_address=0;tp=(void *)1;
    assert(bsp_touch_new(&cfg,&tp) == ESP_ERR_NOT_FOUND && tp == NULL);
    assert(bsp_touch_new(NULL,&tp) == ESP_ERR_INVALID_ARG);
    assert(bsp_touch_new(&cfg,NULL) == ESP_ERR_INVALID_ARG);
    probe_address=0x5d;init_failure=1;tp=(void *)1;
    assert(bsp_touch_new(&cfg,&tp) == ESP_FAIL && tp == NULL);
    init_failure=0;allocation_failure=true;tp=(void *)1;
    assert(bsp_touch_new(&cfg,&tp) == ESP_ERR_NO_MEM && tp == NULL);
    assert(!allocations && !live_devices);
    /* Other BSP users retain IDF's zero/omitted timeout compatibility. */
    esp_lcd_panel_io_i2c_config_t legacy=ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    esp_lcd_panel_io_handle_t io=NULL;
    assert(esp_lcd_new_panel_io_i2c(&bus,&legacy,&io) == ESP_OK);
    assert(((lcd_panel_io_i2c_t *)io)->transaction_timeout_ms == -1);
    esp_lcd_panel_io_del(io);
    legacy.transaction_timeout_ms=-1;
    assert(esp_lcd_new_panel_io_i2c(&bus,&legacy,&io) == ESP_OK);
    assert(((lcd_panel_io_i2c_t *)io)->transaction_timeout_ms == -1);
    esp_lcd_panel_io_del(io);
    legacy.transaction_timeout_ms=-2;
    assert(esp_lcd_new_panel_io_i2c(&bus,&legacy,&io) == ESP_ERR_INVALID_ARG);
    assert(!allocations && !live_devices);
    puts("panel10 BSP + IDF I2C: timeout, NACK/busy recovery and ownership passed");
    return 0;
}
