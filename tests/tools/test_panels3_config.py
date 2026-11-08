# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Compile the production profile against safe and stale generated configs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class Panels3ConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = os.environ.get("CC", "cc")
        if not shutil.which(cls.cc):
            raise unittest.SkipTest("Provide a host C compiler (CC)")

    def compile_config(self, variant, options):
        with tempfile.TemporaryDirectory(prefix="betta-panels3-config-") as temp:
            config = f"#define {variant} 1\n"
            config += "".join(f"#define {key} {value}\n" for key, value in options.items())
            Path(temp, "sdkconfig.h").write_text(config, encoding="utf-8")
            source = '#include "app_config.h"\n'
            source += '_Static_assert(APP_HTTP_TASK_STACK == 14336, "HTTP screenshot stack must remain 14 KiB");\n'
            if variant == "CONFIG_APP_PANEL_VARIANT_S3_480":
                source += '_Static_assert(APP_SCREEN_WIDTH == 480 && APP_SCREEN_HEIGHT == 480, "S3 geometry");\n'
            return subprocess.run(
                [self.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                 "-I", temp, "-I", str(ROOT / "main"), "-x", "c", "-"],
                input=source, capture_output=True, text=True, check=False)

    def assert_safe(self, variant, options):
        result = self.compile_config(variant, options)
        self.assertEqual(result.returncode, 0, result.stderr)

    def assert_rejected(self, options):
        result = self.compile_config("CONFIG_APP_PANEL_VARIANT_S3_480", options)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("panels3 requires Wi-Fi IRAM optimizations disabled", result.stderr)
        self.assertIn("regenerate build/panels3/sdkconfig", result.stderr)

    def test_disabled_kconfig_options_are_absent(self):
        self.assert_safe("CONFIG_APP_PANEL_VARIANT_S3_480", {})

    def test_explicit_zero_values_are_accepted(self):
        self.assert_safe("CONFIG_APP_PANEL_VARIANT_S3_480", {
            "CONFIG_ESP_WIFI_IRAM_OPT": 0, "CONFIG_ESP_WIFI_RX_IRAM_OPT": 0})

    def test_stale_iram_optimization_is_rejected(self):
        self.assert_rejected({"CONFIG_ESP_WIFI_IRAM_OPT": 1})

    def test_stale_rx_iram_optimization_is_rejected(self):
        self.assert_rejected({"CONFIG_ESP_WIFI_RX_IRAM_OPT": 1})

    def test_both_stale_optimizations_are_rejected(self):
        self.assert_rejected({"CONFIG_ESP_WIFI_IRAM_OPT": 1, "CONFIG_ESP_WIFI_RX_IRAM_OPT": 1})

    def test_p4_profiles_do_not_inherit_s3_policy(self):
        for variant in ("CONFIG_APP_PANEL_VARIANT_4INCH_720", "CONFIG_APP_PANEL_VARIANT_10INCH_1280",
                        "CONFIG_APP_PANEL_VARIANT_10INCH_JC", "CONFIG_APP_PANEL_VARIANT_TAB5_1280"):
            with self.subTest(variant=variant):
                self.assert_safe(variant, {"CONFIG_ESP_WIFI_IRAM_OPT": 1, "CONFIG_ESP_WIFI_RX_IRAM_OPT": 1})


if __name__ == "__main__":
    unittest.main()
