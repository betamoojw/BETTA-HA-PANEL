# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Exercise adapter patch migration and the resulting lock behavior with host stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from apply_component_patches import PATCHES, patch_panel10_lock_context

# Original esp_lvgl_adapter lock functions (Apache-2.0, Espressif Systems).
PRISTINE = '''esp_err_t esp_lv_adapter_lock(int32_t timeout_ms)
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

QUIET = PRISTINE.replace(
    '    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) == pdTRUE,\n'
    '                        ESP_ERR_TIMEOUT, TAG, "Failed to acquire LVGL lock");',
    '    /* andre68723/BETTA-HA-PANEL fork patch (tools/apply_component_patches.py): short polling timeouts are expected;\n'
    '     * display_timing records them, so no error log line per attempt. */\n'
    '    if (xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) != pdTRUE) {\n'
    '        return ESP_ERR_TIMEOUT;\n'
    '    }',
)

STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
typedef void *TaskHandle_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define configMAX_TASK_NAME_LEN 16
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_TIMEOUT 2
#define ESP_RETURN_ON_FALSE(condition, error, ...) do { if (!(condition)) return (error); } while (0)
#define ESP_LOGE(tag, format, timeout, name, caller, holder, pc) do { \
    ++logs; (void)(format); (void)(timeout); (void)(name); (void)(caller); (void)(holder); (void)(pc); \
} while (0)
#define PRId32 "d"
static struct { void *lvgl_mutex; } s_ctx;
static int acquired = 1, gives, takes, logs;
static TickType_t last_ticks;
static int task_token;
static int xSemaphoreTakeRecursive(void *mutex, TickType_t ticks) {
    assert(mutex); ++takes; last_ticks = ticks; return acquired;
}
static void xSemaphoreGiveRecursive(void *mutex) { assert(mutex); ++gives; }
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return &task_token; }
static TaskHandle_t xSemaphoreGetMutexHolder(void *mutex) { assert(mutex); return &task_token; }
static const char *pcTaskGetName(void *task) { (void)task; return "ui-task"; }
static size_t strlcpy(char *to, const char *from, size_t size) {
    snprintf(to, size, "%s", from); return strlen(from);
}
#endif
'''

EXERCISE = r'''
int main(void) {
    assert(esp_lv_adapter_lock(20) == ESP_ERR_INVALID_STATE);
    assert(takes == 0);
    s_ctx.lvgl_mutex = &task_token;
    assert(esp_lv_adapter_lock(-1) == ESP_OK && last_ticks == portMAX_DELAY);
    assert(esp_lv_adapter_lock(20) == ESP_OK && last_ticks == 20);
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
    assert(s_lock_owner.depth == 2 && s_lock_owner.task == &task_token);
    assert(strcmp(s_lock_owner.name, "ui-task") == 0);
#endif
    acquired = 0;
    assert(esp_lv_adapter_lock(0) == ESP_ERR_TIMEOUT);
    assert(esp_lv_adapter_lock(20) == ESP_ERR_TIMEOUT && logs == 0);
    assert(esp_lv_adapter_lock(100) == ESP_ERR_TIMEOUT);
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
    assert(logs == 1 && s_lock_owner.depth == 2);
#else
    assert(logs == 0);
#endif
    esp_lv_adapter_unlock();
    esp_lv_adapter_unlock();
    assert(gives == 2);
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
    assert(s_lock_owner.depth == 0);
#endif
    acquired = 1;
    assert(esp_lv_adapter_lock(20) == ESP_OK);
    esp_lv_adapter_unlock();
    assert(gives == 3);
    s_ctx.lvgl_mutex = NULL;
    esp_lv_adapter_unlock();
    assert(gives == 3);
    return 0;
}
'''


class AdapterLockPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="betta-adapter-patch-")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.path = self.folder / "adapter.c"

    def apply_and_check(self, source):
        self.path.write_text(source, encoding="utf-8")
        state, patched = patch_panel10_lock_context(self.path)
        self.assertEqual(state, "applied")
        self.path.write_text(patched, encoding="utf-8")
        self.assertEqual(patch_panel10_lock_context(self.path), ("present", None))
        self.assertEqual(self.path.read_text(encoding="utf-8"), patched)
        return patched

    def test_fresh_and_upstream_quiet_sources_converge(self):
        self.assertEqual(self.apply_and_check(PRISTINE), self.apply_and_check(QUIET))

    def test_patch_does_not_swallow_unrecognized_source(self):
        self.path.write_text(PRISTINE.replace("xSemaphoreTakeRecursive", "new_lock_api"), encoding="utf-8")
        original = self.path.read_bytes()
        self.assertEqual(patch_panel10_lock_context(self.path), ("missing", None))
        self.assertEqual(self.path.read_bytes(), original)

    def test_cached_component_migration_when_available(self):
        source = Path(os.environ.get("BETTA_ADAPTER_SOURCE", ROOT / "managed_components/espressif__esp_lvgl_adapter/src/adapter/esp_lv_adapter.c"))
        if not source.exists():
            self.skipTest("No fetched adapter source; set BETTA_ADAPTER_SOURCE to test a cached version")
        original = source.read_text(encoding="utf-8")
        self.path.write_text(original, encoding="utf-8")
        state, patched = patch_panel10_lock_context(self.path)
        self.assertIn(state, ("applied", "present"))
        if state == "applied":
            self.path.write_text(patched, encoding="utf-8")
        self.assertEqual(patch_panel10_lock_context(self.path), ("present", None))
        self.assertEqual(source.read_text(encoding="utf-8"), original)

    def test_only_one_lock_patch_runs_and_rgb_patch_remains(self):
        lock_patches = [entry for entry in PATCHES if entry[1] == "src/adapter/esp_lv_adapter.c"]
        self.assertEqual(len(lock_patches), 1)
        self.assertIs(lock_patches[0][3], patch_panel10_lock_context)
        self.assertTrue(any(entry[1] == "src/display/bridge/v9/lvgl_bridge_v9.c" for entry in PATCHES))
        self.assertFalse(any(entry[0] == "espressif__m5stack_tab5" for entry in PATCHES))

    def test_generated_lock_preserves_recursion_timeout_and_recovery(self):
        cc = os.environ.get("CC", "cc")
        if not shutil.which(cc):
            self.skipTest("Provide a host C compiler through CC")
        patched = self.apply_and_check(PRISTINE)
        self.path.write_text(STUBS + patched + EXERCISE, encoding="utf-8")
        for panel10 in (0, 1):
            with self.subTest(panel10=panel10):
                executable = self.folder / (f"lock-{panel10}.exe" if os.name == "nt" else f"lock-{panel10}")
                build = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                        f"-DCONFIG_APP_PANEL_VARIANT_10INCH_1280={panel10}",
                                        str(self.path), "-o", str(executable)], capture_output=True, text=True)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=5)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
