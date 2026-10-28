#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Exercise camera controls in the real editor with local synthetic responses.

Requires the screenshots group's Playwright installation and Chromium. No panel,
Home Assistant or IP camera is contacted. Optional --output retains screenshots.
"""
import argparse
import json
import tempfile
import threading
from pathlib import Path
from playwright.sync_api import sync_playwright
import editor_local


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', help='installed Chromium executable')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='betta-camera-editor-') as tmp:
        server = editor_local.build_server(0, 'panel10', state_path=Path(tmp) / 'state.json')
        server.local_store.responses['/api/settings']['ui']['language'] = 'en'
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=args.browser)
                page = browser.new_page(viewport={'width': 1440, 'height': 1000})
                errors = []
                page.on('pageerror', lambda e: errors.append(str(e)))
                page.goto(f'http://127.0.0.1:{server.server_port}/', wait_until='networkidle')
                page.wait_for_function('window.localEditorStartupComplete === true')
                page.evaluate('setActivePane("settings");setActiveSettingsSection("settingsCameraSection");cameraEditor.poll()')
                page.locator('#cam_enabled').wait_for()
                assert not page.locator('#cam_enabled').is_checked()
                assert page.locator('#camOutputFields').is_hidden()
                page.locator('#cam_enabled').check()
                page.locator('#cam_stream_enabled').check()
                page.locator('#cam_jpeg_quality').fill('73')
                page.locator('#cam_motion_enabled').check()
                page.locator('#camZones button').click()
                page.locator('#cam_zone_0_w').fill('50')
                page.locator('#cam_zone_0_x').fill('25')
                page.locator('#camSources button').click()
                page.locator('#cam_source_0_name').fill('Synthetic camera')
                page.locator('#cam_source_0_snapshot_url').fill('http://synthetic.invalid/frame.jpg')
                page.locator('#cam_source_0_username').fill('test-viewer')
                page.locator('#cam_source_0_password').fill('synthetic-only')
                page.locator('#camSave').click()
                page.wait_for_function('document.getElementById("camMessage").textContent.includes("saved")')
                saved = server.local_store.responses['/api/camera/config']
                assert saved['local']['jpeg_quality'] == 73
                assert saved['local']['motion']['zones'] == [{'x': 25, 'y': 0, 'w': 50, 'h': 100}]
                assert 'password' not in saved['sources'][0]
                page.locator('#cam_enabled').uncheck()
                page.locator('#camSave').click()
                page.wait_for_function('document.getElementById("camMessage").textContent.includes("saved")')
                page.reload(wait_until='networkidle')
                page.evaluate('setActivePane("settings");setActiveSettingsSection("settingsCameraSection");cameraEditor.poll()')
                page.locator('#cam_enabled').wait_for()
                assert not page.locator('#cam_enabled').is_checked()
                page.locator('#cam_enabled').check()
                assert page.locator('#cam_jpeg_quality').input_value() == '73'
                page.locator('#camImageFields').evaluate('(e)=>e.open=true')
                # Inspect every visible control against its group, not just page overflow.
                for width in (1440, 390):
                    page.set_viewport_size({'width': width, 'height': 1000})
                    page.wait_for_timeout(200)
                    issues = page.evaluate('''() => {
                        const issues=[];
                        for (const e of document.querySelectorAll('#settingsCameraSection input, #settingsCameraSection select, #settingsCameraSection button')) {
                            if (!e.getClientRects().length) continue;
                            const r=e.getBoundingClientRect(), p=e.parentElement.getBoundingClientRect();
                            if(r.left<p.left-2||r.right>p.right+2) issues.push(e.id||e.textContent);
                        }
                        if(document.documentElement.scrollWidth>innerWidth+2)issues.push('page overflow');
                        return issues;
                    }''')
                    assert not issues, (width, issues)
                    if args.output:
                        args.output.mkdir(parents=True, exist_ok=True)
                        page.screenshot(path=str(args.output / f'camera-editor-{width}.png'), full_page=True)
                assert not errors, errors
                browser.close()
                print('Camera editor: PASS (real browser events, saved JSON, reload, privacy defaults, 1440/390 px bounds)')
        finally:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    main()
