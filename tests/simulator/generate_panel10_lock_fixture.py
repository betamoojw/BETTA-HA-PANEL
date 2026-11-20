# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Compile the production lock and refresh paths without a device SDK."""
from pathlib import Path
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import apply_component_patches


def function(source, signature):
    start = source.index("\n" + signature + "\n{") + 1
    end = source.index("\n}\n", start) + len("\n}")
    return source[start:end]


def main(output):
    runtime = (ROOT / "main/ui/ui_runtime.c").read_text(encoding="utf-8")
    ha = (ROOT / "main/ha/ha_client.c").read_text(encoding="utf-8")
    cache_end = runtime.index("static ui_topbar_cache_t s_topbar_cache")
    cache_start = runtime.rindex("typedef struct {", 0, cache_end)
    cache_end = runtime.index(";", cache_end) + 1
    refresh = runtime.index("static ui_periodic_refresh_t s_periodic_refresh")
    refresh_end = runtime.index(";", refresh) + 1

    # Use the replacement payload when managed_components is absent in host CI.
    with patch.object(apply_component_patches, "replace") as replace:
        apply_component_patches.patch_panel10_lock_context(None)
        _, _, adapter = replace.call_args.args

    parts = [
        adapter,
        function(ha, "bool ha_client_try_ui_status(bool *connected, bool *initial_sync_done)"),
        runtime[cache_start:cache_end],
        function(runtime, "static void ui_runtime_refresh_topbar(void)"),
        function(runtime, "static void ui_runtime_periodic_refresh(void)"),
        runtime[refresh:refresh_end],
    ]
    Path(output).write_text("\n\n".join(parts) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1])
