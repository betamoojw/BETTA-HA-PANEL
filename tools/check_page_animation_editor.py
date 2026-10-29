#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Check animation preference events and layout in the synthetic web editor.

Requires Playwright/Chromium. No device, camera or Home Assistant is contacted.
"""
import argparse
import copy
import tempfile
import threading
from pathlib import Path
from playwright.sync_api import sync_playwright
import editor_local


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='betta-transition-editor-') as tmp:
        server = editor_local.build_server(0, 'panel10', state_path=Path(tmp) / 'state.json')
        settings = server.local_store.responses['/api/settings']
        settings['ui']['language'] = 'en'
        original_layout = copy.deepcopy(server.local_store.responses['/api/layout'])
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=args.browser)
                page = browser.new_page(viewport={'width': 1440, 'height': 1000})
                errors = []
                page.on('pageerror', lambda error: errors.append(str(error)))

                def open_design():
                    page.goto(f'http://127.0.0.1:{server.server_port}/', wait_until='networkidle')
                    page.wait_for_function('window.localEditorStartupComplete === true')
                    page.evaluate('setActivePane("settings");setActiveSettingsSection(document.getElementById("settingsAppearanceHeading").closest("section").id)')
                    page.locator('#settingsPageAnimations').wait_for()

                open_design()
                checkbox = page.locator('#settingsPageAnimations')
                assert checkbox.is_checked()
                for enabled in [False, True, False]:
                    with page.expect_response(lambda r: r.url.endswith('/api/settings') and r.request.method == 'PUT') as response:
                        checkbox.set_checked(enabled)
                    assert response.value.ok
                    payload = response.value.request.post_data_json
                    assert payload['ui']['page_animations'] is enabled and payload['reboot'] is False
                    assert settings['ui']['page_animations'] is enabled
                    assert page.evaluate('settingsForBackup(editor.settings).ui.page_animations') is not None
                    open_design()
                    assert checkbox.is_checked() is enabled
                    assert page.evaluate('settingsForBackup(editor.settings).ui.page_animations') is enabled
                with page.expect_response(lambda r: r.url.endswith('/api/settings') and r.request.method == 'PUT'):
                    page.locator('#settingsAppearanceResetBtn').click()
                assert checkbox.is_checked() and settings['ui']['page_animations'] is True
                assert settings['ui']['language'] == 'en'
                assert server.local_store.responses['/api/layout'] == original_layout
                for width in [1440, 390]:
                    page.set_viewport_size({'width': width, 'height': 1000})
                    checkbox.scroll_into_view_if_needed()
                    issues = page.evaluate('''() => {
                        const section = document.getElementById('settingsPageAnimations').closest('section');
                        const issues = [];
                        for (const input of section.querySelectorAll('input, select, button')) {
                            if (!input.offsetWidth || input.closest('.visually-hidden')) continue;
                            const box = input.getBoundingClientRect();
                            const group = input.closest('.form')?.getBoundingClientRect();
                            if (group && (box.left < group.left - 2 || box.right > group.right + 2)) issues.push(input.id);
                        }
                        const label = document.getElementById('settingsPageAnimations').closest('label');
                        const input = label.querySelector('input').getBoundingClientRect();
                        const text = label.querySelector('span').getBoundingClientRect();
                        if (input.right > text.left + 1) issues.push('overlapping label');
                        if (document.documentElement.scrollWidth > innerWidth + 2) issues.push('page overflow');
                        return issues;
                    }''')
                    assert not issues, (width, issues)
                    if args.output:
                        args.output.mkdir(parents=True, exist_ok=True)
                        page.screenshot(path=str(args.output / f'design-{width}.png'), full_page=True)
                assert not errors, errors
                browser.close()
                print('Page animation editor: PASS (events, persistence, reload, backup, reset, 1440/390 px bounds)')
        finally:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    main()
