#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
"""Re-applies the small edits this project needs in components fetched into managed_components/.

The component manager replaces managed_components/ whenever a manifest changes, which used to wipe these
edits by hand. The root CMakeLists.txt now runs this on every configure (right after project()), so there
is nothing to remember to run by hand any more; this file is for checking or applying it outside of a
build. It is safe to run any time: an edit that is already in place is reported and left alone, one whose
place cannot be found (a new component version) is reported as a warning and the exit code is 1.

  tools/apply_component_patches.py            apply
  tools/apply_component_patches.py --check    only report, change nothing
"""
import os
import re
import sys
from pathlib import Path
from patch_websocket_transport import patch_websocket_transport

ROOT = Path(__file__).resolve().parent.parent
MC = Path(os.environ.get("COMPONENTS_DIR", ROOT / "managed_components"))


def replace(path, old, new):
    """Text edit: old -> new. Returns 'applied' / 'present' / 'missing'."""
    text = path.read_text(encoding="utf-8")
    if new in text:
        return "present", None
    if old not in text:
        return "missing", None
    return "applied", text.replace(old, new, 1)


def replace_ppa_legacy(path, old, new):
    """The bounded-wait patch may already have the newer diagnostic hook signature."""
    text = path.read_text(encoding="utf-8")
    upgraded = new.replace(
        "__attribute__((weak)) void lvgl_port_ppa_timeout_hook(void) {}",
        "__attribute__((weak)) void lvgl_port_ppa_timeout_hook(uint32_t width, uint32_t height, uint32_t x, uint32_t y,\n"
        "    uint32_t rotation, uintptr_t input, uintptr_t output, uint32_t capacity) {}",
    ).replace(
        "        lvgl_port_ppa_timeout_hook();",
        "        lvgl_port_ppa_timeout_hook(w, h, input_x, input_y, (uint32_t)rotate_cfg->rotation * 90U,\n"
        "            (uintptr_t)rotate_cfg->in_buff, (uintptr_t)ppa_ctx->buffer, ppa_ctx->buffer_size);",
    )
    if upgraded in text:
        return "present", None
    return replace(path, old, new)


def patch_panel10_lock_context(path):
    old = '''esp_err_t esp_lv_adapter_lock(int32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_ctx.lvgl_mutex, ESP_ERR_INVALID_STATE, TAG, "Adapter LVGL mutex not initialized");

    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Failed to acquire LVGL lock");

    return ESP_OK;
}

void esp_lv_adapter_unlock(void)
{
    if (s_ctx.lvgl_mutex) {
        xSemaphoreGiveRecursive(s_ctx.lvgl_mutex);
    }
}'''
    new = '''/* Panel10 lock context from the actual adapter mutex, including lvgl_worker. */
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
typedef struct {
    TaskHandle_t task;
    uintptr_t caller;
    unsigned depth;
    char name[configMAX_TASK_NAME_LEN];
} betta_lvgl_owner_t;

static portMUX_TYPE s_lock_context_mux = portMUX_INITIALIZER_UNLOCKED;
static betta_lvgl_owner_t s_lock_owner;

static void betta_lvgl_lock_acquired(uintptr_t caller)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&s_lock_context_mux);
    if (s_lock_owner.depth == 0 || s_lock_owner.task != task) {
        s_lock_owner.task = task;
        s_lock_owner.caller = caller;
        s_lock_owner.depth = 0;
        strlcpy(s_lock_owner.name, pcTaskGetName(NULL), sizeof(s_lock_owner.name));
    }
    ++s_lock_owner.depth;
    portEXIT_CRITICAL(&s_lock_context_mux);
}

static void betta_lvgl_lock_timeout(int32_t timeout_ms, uintptr_t caller)
{
    /* Short polling timeouts are expected; display_timing counts them. */
    if (timeout_ms >= 0 && timeout_ms <= 20) return;
    /* Task names are copied on acquisition so this never dereferences another task. */
    TaskHandle_t holder = xSemaphoreGetMutexHolder(s_ctx.lvgl_mutex);
    portENTER_CRITICAL(&s_lock_context_mux);
    betta_lvgl_owner_t owner = s_lock_owner;
    portEXIT_CRITICAL(&s_lock_context_mux);
    bool known = holder && owner.task == holder && owner.depth != 0;
    ESP_LOGE(TAG, "Failed to acquire LVGL lock (timeout_ms=%" PRId32 " waiter=%s caller=%p holder=%s holder_pc=%p)",
             timeout_ms, pcTaskGetName(NULL), (void *)caller, known ? owner.name : "unavailable",
             (void *)(known ? owner.caller : 0));
}

esp_err_t betta_lv_adapter_lock_at(int32_t timeout_ms, uintptr_t caller)
#else
esp_err_t esp_lv_adapter_lock(int32_t timeout_ms)
#endif
{
    ESP_RETURN_ON_FALSE(s_ctx.lvgl_mutex, ESP_ERR_INVALID_STATE, TAG, "Adapter LVGL mutex not initialized");

    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    BaseType_t acquired = xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks);
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
    if (acquired != pdTRUE) {
        betta_lvgl_lock_timeout(timeout_ms, caller);
        return ESP_ERR_TIMEOUT;
    }
    betta_lvgl_lock_acquired(caller);
#else
    if (acquired != pdTRUE) return ESP_ERR_TIMEOUT;
#endif
    return ESP_OK;
}

#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
__attribute__((noinline)) esp_err_t esp_lv_adapter_lock(int32_t timeout_ms)
{
    return betta_lv_adapter_lock_at(timeout_ms, (uintptr_t)__builtin_return_address(0));
}
#endif

void esp_lv_adapter_unlock(void)
{
    if (s_ctx.lvgl_mutex) {
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
        /* Update metadata before handing the mutex to another core. */
        TaskHandle_t task = xTaskGetCurrentTaskHandle();
        portENTER_CRITICAL(&s_lock_context_mux);
        if (s_lock_owner.task == task && s_lock_owner.depth != 0) --s_lock_owner.depth;
        portEXIT_CRITICAL(&s_lock_context_mux);
#endif
        xSemaphoreGiveRecursive(s_ctx.lvgl_mutex);
    }
}'''
    text = path.read_text(encoding="utf-8")
    if new in text:
        return "present", None
    # Upgrade existing dev and upstream patch results as well as pristine sources.
    # A component-manager cache can retain either state across reconfiguration.
    legacy = new.replace(
        "    /* Short polling timeouts are expected; display_timing counts them. */\n"
        "    if (timeout_ms >= 0 && timeout_ms <= 20) return;\n", "",
    ).replace(
        "    if (acquired != pdTRUE) return ESP_ERR_TIMEOUT;",
        "    ESP_RETURN_ON_FALSE(acquired == pdTRUE,\n"
        "                        ESP_ERR_TIMEOUT, TAG, \"Failed to acquire LVGL lock\");",
    )
    quiet = old.replace(
        "    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) == pdTRUE,\n"
        "                        ESP_ERR_TIMEOUT, TAG, \"Failed to acquire LVGL lock\");",
        "    /* andre68723/BETTA-HA-PANEL fork patch (tools/apply_component_patches.py): short polling timeouts are expected;\n"
        "     * display_timing records them, so no error log line per attempt. */\n"
        "    if (xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) != pdTRUE) {\n"
        "        return ESP_ERR_TIMEOUT;\n"
        "    }",
    )
    for previous in (old, legacy, quiet):
        if previous in text:
            return "applied", text.replace(previous, new, 1)
    return "missing", None


def ensure_requires(path, keyword, tokens):
    """Make sure `tokens` are in the `keyword` list (REQUIRES / PRIV_REQUIRES) of idf_component_register()."""
    text = path.read_text(encoding="utf-8")
    m = re.search(r"^(\s*%s\s+)([^\n]*)$" % re.escape(keyword), text, re.M)
    if not m:
        return "missing", None
    have = m.group(2).split()
    add = [t for t in tokens if t not in have]
    if not add:
        return "present", None
    return "applied", text[: m.start(2)] + " ".join(have + add) + text[m.end(2):]


# (component directory, file inside it, what, edit function, its arguments[, optional: the place may not exist])
PATCHES = [
    # BETTA already owns the group-address registry. Route group telegrams directly
    # to it without duplicating 512 entries in the transport's fixed callback table.
    ("esp_knx_ip", "private_include/esp_knx_ip_internal/context.hpp",
     "KNX: per-handle group listener",
     replace, ("    esp_netif_t *netif = nullptr;",
               "    esp_knx_ip_telegram_callback_t group_listener = nullptr;\n"
               "    esp_netif_t *netif = nullptr;")),
    ("esp_knx_ip", "src/esp_knx_ip.cpp", "KNX: dispatch to the application registry",
     replace, ("    esp_knx_ip_telegram_callback_t callback = nullptr;",
               "    if (context->group_listener) {\n"
               "        context->group_listener(&telegram, nullptr);\n"
               "        return;\n    }\n"
               "    esp_knx_ip_telegram_callback_t callback = nullptr;")),
    ("esp_knx_ip", "src/esp_knx_ip.cpp", "KNX: install group listener before start",
     replace, ('extern "C" esp_err_t esp_knx_ip_destroy(',
               'extern "C" esp_err_t betta_knx_route_create(const esp_knx_ip_config_t *config,\n'
               '    esp_knx_ip_telegram_callback_t listener, esp_knx_ip_handle_t *out)\n'
               '{\n    esp_err_t result = esp_knx_ip_create(config, out);\n'
               '    if (result == ESP_OK) (*out)->group_listener = listener;\n'
               '    return result;\n}\n\nextern "C" esp_err_t esp_knx_ip_destroy(')),
    ("espressif__esp_lvgl_adapter", "src/adapter/esp_lv_adapter.c",
     "esp_lvgl_adapter: retain Panel10 lock context without logging expected polling timeouts",
     patch_panel10_lock_context, ()),
    ("espressif__esp_websocket_client", "esp_websocket_client.c",
     "esp_websocket_client: serialize TLS polling/close and bound stalled receives",
     patch_websocket_transport, ()),
    ("espressif__bmi270", "src/bmi270.c",
     "bmi270: bounded I2C reads on the shared Tab5 touch/backlight bus",
     replace, ("i2c_master_transmit_receive(dev_handle->i2c_handle, &reg, 1, data, data_len, -1)",
               "i2c_master_transmit_receive(dev_handle->i2c_handle, &reg, 1, data, data_len, 100) /* BETTA: 100 ms timeout */")),
    ("espressif__bmi270", "src/bmi270.c",
     "bmi270: bounded I2C writes on the shared Tab5 touch/backlight bus",
     replace, ("i2c_master_transmit(dev_handle->i2c_handle, packet, packet_len, -1)",
               "i2c_master_transmit(dev_handle->i2c_handle, packet, packet_len, 100) /* BETTA: 100 ms timeout */")),
    ("espressif__esp_hosted", "host/eh_host_config/include/eh_host_port_master_config.h",
     "esp_hosted: SDIO byte mode TX (workaround for esp-hosted-mcu#167), all P4 variants",
     replace, ("#define EH_HOST_PORT_SDIO_TX_BLOCK_ONLY_XFER (1)", "#define EH_HOST_PORT_SDIO_TX_BLOCK_ONLY_XFER (0)")),
    ("espressif__esp_hosted", "host/mcu/eh_host_mcu_transport/src/eh_host_bus_sdio.c",
     "esp_hosted: bounded eh_host_bus_tx() queue wait instead of an infinite one, all P4 variants",
     replace, (
        "    eh_host_port_queue_send(to_slave_queue[pkt_prio], buf_handle, EH_HOST_PORT_WAIT_FOREVER);\n"
        "    eh_host_port_sem_post(sem_to_slave_queue);\n",
        "    /* andre68723/BETTA-HA-PANEL fork patch (tools/apply_component_patches.py), see docs/release-notes.md\n"
        "     * \"ESP-IDF 6.1 and the ESP32-C6 link\": an infinite wait here froze lwIP's own task (and with it the whole\n"
        "     * network stack) whenever to_slave_queue[pkt_prio] stayed full, e.g. a burst of concurrent outbound traffic\n"
        "     * faster than the SDIO write task drains it. A bounded wait turns that into a dropped packet (TCP retransmits),\n"
        "     * the same graceful degradation used above when a frame is rejected. The timeout is in milliseconds. */\n"
        "    if (eh_host_port_queue_send(to_slave_queue[pkt_prio], buf_handle, 300) != EH_HOST_PORT_OK) {\n"
        "        ESP_LOGW(TAG, \"TX: to_slave_queue[%u] full for 300 ms, dropping pkt (len=%u) instead of blocking forever\",\n"
        "                 pkt_prio, buf_handle->payload_len);\n"
        "        EH_HOST_PORT_FREE_PTR_WITH_FUNC(buf_handle->free_buf_handle, buf_handle->priv_buffer_handle);\n"
        "        return ESP_FAIL;\n"
        "    }\n"
        "    eh_host_port_sem_post(sem_to_slave_queue);\n")),
    ("espressif__esp_lvgl_adapter", "src/display/bridge/v9/lvgl_bridge_v9.c",
     "RGB cache-safe ISR: keep the v9 bridge callback context in internal RAM",
     replace, ("    esp_lv_adapter_display_bridge_v9_t *impl = calloc(1, sizeof(*impl));",
               "    /* andre68723/BETTA-HA-PANEL: the RGB driver rejects PSRAM user_ctx\n"
               "     * with LCD_RGB_ISR_IRAM_SAFE. Frame/draw buffers still use PSRAM. */\n"
               "    esp_lv_adapter_display_bridge_v9_t *impl;\n"
               "#if CONFIG_SOC_LCD_RGB_SUPPORTED && CONFIG_LCD_RGB_ISR_IRAM_SAFE\n"
               "    if (cfg->base.profile.interface == ESP_LV_ADAPTER_PANEL_IF_RGB) {\n"
               "        impl = heap_caps_calloc(1, sizeof(*impl), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n"
               "    } else\n"
               "#endif\n"
               "    {\n"
               "        impl = calloc(1, sizeof(*impl));\n"
               "    }")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: PPA rotation waits at most 1 s for the hardware (a missing done interrupt hung the LVGL task, paneljc 2026-10-05), part 1",
     replace_ppa_legacy, ("#include \"lcd_ppa.h\"\n",
               "#include \"lcd_ppa.h\"\n"
               "#include \"freertos/FreeRTOS.h\"\n"
               "#include \"freertos/semphr.h\"\n"
               "#include \"esp_log.h\"\n"
               "\n"
               "/* andre68723/BETTA-HA-PANEL fork patch (tools/apply_component_patches.py): the blocking PPA call waits with\n"
               " * portMAX_DELAY for a done interrupt. When it never comes the LVGL task, and with it the display lock, hangs for\n"
               " * good. The rotation now runs non-blocking and waits 1 s for the done semaphore; on a timeout the application's\n"
               " * hook (main/util/ui_watchdog_esp.c) writes a core dump and restarts. */\n"
               "__attribute__((weak)) void lvgl_port_ppa_timeout_hook(void) {}\n")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: bounded PPA wait, part 2 (context)",
     replace, ("    ppa_srm_color_mode_t color_mode;\n};",
               "    ppa_srm_color_mode_t color_mode;\n    SemaphoreHandle_t    done_sem;\n};")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: bounded PPA wait, part 3 (done callback)",
     replace, ("#define PPA_LCD_ENABLE_CB   0", "#define PPA_LCD_ENABLE_CB   1")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: bounded PPA wait, part 4 (callback gives the semaphore)",
     replace, ("{\n    return false;\n}\n#endif",
               "{\n    BaseType_t woken = pdFALSE;\n    lvgl_port_ppa_t *ctx = (lvgl_port_ppa_t *)user_data;\n"
               "    if (ctx && ctx->done_sem) xSemaphoreGiveFromISR(ctx->done_sem, &woken);\n    return woken == pdTRUE;\n}\n#endif")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: bounded PPA wait, part 5 (create the semaphore)",
     replace, ("    ppa_ctx->color_mode = cfg->color_mode;\n",
               "    ppa_ctx->done_sem = xSemaphoreCreateBinary();\n    ESP_GOTO_ON_FALSE(ppa_ctx->done_sem, ESP_ERR_NO_MEM, err, TAG, \"PPA semaphore\");\n"
               "    ppa_ctx->color_mode = cfg->color_mode;\n")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: bounded PPA wait, part 6 (non-blocking operation, timed wait)",
     replace_ppa_legacy, ("        .mode = rotate_cfg->ppa_mode,\n        .user_data = rotate_cfg->user_data,\n    };\n\n    return ppa_do_scale_rotate_mirror(ppa_ctx->srm_handle, &srm_oper_config);",
               "        .mode = PPA_TRANS_MODE_NON_BLOCKING,\n        .user_data = ppa_ctx,\n    };\n\n"
               "    (void)xSemaphoreTake(ppa_ctx->done_sem, 0); /* drop a stale completion */\n"
               "    esp_err_t err = ppa_do_scale_rotate_mirror(ppa_ctx->srm_handle, &srm_oper_config);\n"
               "    if (err != ESP_OK) return err;\n"
               "    if (xSemaphoreTake(ppa_ctx->done_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {\n"
               "        ESP_LOGE(TAG, \"PPA rotation did not finish within 1 s\");\n"
               "        lvgl_port_ppa_timeout_hook();\n"
               "        return ESP_ERR_TIMEOUT;\n"
               "    }\n    return ESP_OK;")),

    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: preserve PPA input position for crash evidence",
     replace, ("    const int h = rotate_cfg->area.y2 - rotate_cfg->area.y1 + 1;",
               "    const int h = rotate_cfg->area.y2 - rotate_cfg->area.y1 + 1;\n"
               "    const uint32_t input_x = rotate_cfg->area.x1;\n"
               "    const uint32_t input_y = rotate_cfg->area.y1;")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: timeout hook receives the failed PPA operation",
     replace, ("__attribute__((weak)) void lvgl_port_ppa_timeout_hook(void) {}",
               "__attribute__((weak)) void lvgl_port_ppa_timeout_hook(uint32_t width, uint32_t height, uint32_t x, uint32_t y,\n"
               "    uint32_t rotation, uintptr_t input, uintptr_t output, uint32_t capacity) {}")),
    ("espressif__esp_lvgl_port", "src/common/ppa/lcd_ppa.c",
     "esp_lvgl_port: pass PPA geometry, rotation and buffer data on timeout",
     replace, ("        lvgl_port_ppa_timeout_hook();",
               "        lvgl_port_ppa_timeout_hook(w, h, input_x, input_y, (uint32_t)rotate_cfg->rotation * 90U,\n"
               "            (uintptr_t)rotate_cfg->in_buff, (uintptr_t)ppa_ctx->buffer, ppa_ctx->buffer_size);")),
]


def main():
    check_only = "--check" in sys.argv[1:]
    problems = 0
    for component, rel, what, fn, args, *optional in PATCHES:
        path = MC / component / rel
        if not path.exists():
            print(f"skip     {what}: {component} is not fetched")
            continue
        state, new_text = fn(path, *args)
        if state == "missing" and optional and optional[0]:
            print(f"n/a      {what} (name not used in this version)")
            continue
        if state == "applied" and not check_only:
            path.write_text(new_text, encoding="utf-8")
        label = {"applied": "would apply" if check_only else "applied", "present": "present", "missing": "MISSING"}[state]
        print(f"{label:9s}{what}")
        if state == "missing" or (state == "applied" and check_only):
            problems += 1
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
