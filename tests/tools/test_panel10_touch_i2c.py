# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Compile production LCD-X touch code and IDF I2C paths with controlled HAL/RTOS failures."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests/host/panel10_touch"


def function(source, name):
    """Keep the function body verbatim, including its error/cleanup branches."""
    match = re.search(r"^(?:static )?(?:esp_err_t|void|touch_read_action_t) " + re.escape(name) + r"\([^;]+?\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(f"Production function {name} not found")
    depth = 1
    pos = match.end()
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[match.start():pos]


def without_includes(source):
    return re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)


class Panel10TouchI2CTest(unittest.TestCase):
    def setUp(self):
        self.cc = os.environ.get("CC", "cc")
        if not shutil.which(self.cc):
            self.skipTest("Provide a host C compiler through CC")
        self.temp = tempfile.TemporaryDirectory(prefix="betta-panel10-touch-")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def run_c(self, source):
        code = self.folder / "test.c"
        code.write_text(source, encoding="utf-8")
        output = self.folder / ("test.exe" if os.name == "nt" else "test")
        build = subprocess.run([self.cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                "-I", str(FIXTURES), str(code), "-o", str(output)], capture_output=True, text=True)
        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        run = subprocess.run([str(output)], capture_output=True, text=True, timeout=5)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_bsp_timeout_reaches_sdk_and_recovers_after_nack_busy_or_timeout(self):
        candidates = [os.environ.get("BETTA_IDF_SOURCE"), os.environ.get("IDF_PATH"),
                      ROOT / ".local/toolchains/idf611"]
        sdk = next((Path(p) for p in candidates if p and
                    (Path(p) / "components/esp_lcd/i2c/esp_lcd_panel_io_i2c.c").is_file()), None)
        if sdk is None:
            self.skipTest("Provide ESP-IDF through BETTA_IDF_SOURCE or IDF_PATH")
        panel_io = (sdk / "components/esp_lcd/i2c/esp_lcd_panel_io_i2c.c").read_text()
        self.assertIn("transaction_timeout_ms", panel_io, "This regression requires the IDF finite-timeout API")
        driver = (sdk / "components/esp_driver_i2c/i2c_master.c").read_text()
        bsp = (ROOT / "third_party/waveshare__esp32_p4_wifi6_touch_lcd_x/esp32_p4_wifi6_touch_lcd_x.c").read_text()
        fixture = (FIXTURES / "i2c.c").read_text()
        source = fixture.replace("/* SDK_SEND_COMMANDS */", function(driver, "s_i2c_send_commands"))
        source = source.replace("/* SDK_SYNC_TRANSACTION */", function(driver, "s_i2c_synchronous_transaction"))
        source = source.replace("/* SDK_PANEL_IO */", without_includes(panel_io))
        source = source.replace("/* BSP_TOUCH_NEW */", function(bsp, "bsp_touch_new"))
        self.run_c(source)

    def test_touch_registration_lock_cleanup_and_retry(self):
        touch = (ROOT / "main/drivers/touch_init_panel10.c").read_text()
        fixture = (FIXTURES / "lifecycle.c").read_text()
        self.run_c(fixture.replace("/* TOUCH_INIT */", without_includes(touch)))

    def test_adapter_unknown_scans_hold_cancel_and_require_clean_release(self):
        path = Path(os.environ.get("BETTA_TOUCH_ADAPTER_SOURCE", ROOT /
                    "managed_components/espressif__esp_lvgl_adapter/src/input/esp_lv_adapter_input_touch.c"))
        if not path.is_file():
            self.skipTest("Provide the fetched touch adapter through BETTA_TOUCH_ADAPTER_SOURCE")
        source = path.read_text()
        enum = re.search(r"typedef enum \{[^}]+\} touch_read_action_t;", source).group()
        policy = function(source, "touch_read_update")
        self.run_c('''#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NOT_FINISHED 4
#define TOUCH_READ_RETRY_LIMIT 3
''' + enum + policy + '''
int main(void) {
    uint8_t failures=0;
    assert(touch_read_update(&failures,ESP_OK,1,true) == TOUCH_READ_PROCESS);
    assert(touch_read_update(&failures,ESP_ERR_NOT_FINISHED,0,true) == TOUCH_READ_HOLD);
    assert(touch_read_update(&failures,ESP_OK,1,true) == TOUCH_READ_PROCESS && failures == 0);
    for(unsigned i=0;i<3;i++)
        assert(touch_read_update(&failures,ESP_ERR_NOT_FINISHED,0,true) == TOUCH_READ_HOLD);
    assert(touch_read_update(&failures,ESP_ERR_NOT_FINISHED,0,true) == TOUCH_READ_CANCEL);
    assert(touch_read_update(&failures,ESP_OK,1,false) == TOUCH_READ_HOLD);
    assert(touch_read_update(&failures,ESP_OK,0,false) == TOUCH_READ_HOLD && failures == 0);
    assert(touch_read_update(&failures,ESP_OK,1,false) == TOUCH_READ_PROCESS);
    failures=0;
    for(unsigned i=0;i<8;i++)
        assert(touch_read_update(&failures,ESP_ERR_NOT_FINISHED,0,false) == TOUCH_READ_HOLD);
    return 0;
}''')


if __name__ == "__main__":
    unittest.main()
