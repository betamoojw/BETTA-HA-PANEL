#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Exercise clock styles in the synthetic editor; requires Playwright/Chromium.

No panel or Home Assistant is contacted. Optional captures use public demo data.
"""
import argparse
import copy
import datetime
import json
import tempfile
import threading
from pathlib import Path
from playwright.sync_api import sync_playwright
import editor_local


def check(browser_path, out):
    out.mkdir(parents=True, exist_ok=True)
    server = editor_local.build_server(0, 'panel10', state_path=out / 'editor-state.json', reset=True)
    settings = server.local_store.responses['/api/settings']
    settings['ui']['language'] = 'en'
    settings.setdefault('hardware', {}).update(saver_mode='clock', clock={
        'analog': True, 'format_24h': False, 'show_date': False, 'show_seconds': False,
        'brightness': 17, 'off_enabled': True, 'off_seconds': 600, 'start_seconds': 60})
    original_layout = copy.deepcopy(server.local_store.responses['/api/layout'])
    threading.Thread(target=server.serve_forever, daemon=True).start()
    checks = []
    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(executable_path=browser_path)
            page = browser.new_page(viewport={'width': 1440, 'height': 1000}, timezone_id='Pacific/Honolulu')
            errors = []
            page.on('pageerror', lambda error: errors.append(str(error)))
            def open_display():
                page.goto(f'http://127.0.0.1:{server.server_port}/', wait_until='networkidle')
                page.wait_for_function('window.localEditorStartupComplete === true')
                page.evaluate('setActivePane("settings");setActiveSettingsSection(document.getElementById("settingsClockStyle").closest("section").id)')
                page.locator('#settingsClockStyle').wait_for()
            def select(selector, value):
                with page.expect_response(lambda r: r.url.endswith('/api/settings') and r.request.method == 'PUT') as response:
                    page.locator(selector).select_option(value)
                assert response.value.ok
                return response.value.request.post_data_json
            picker = page.locator('#settingsTimezoneSearch')
            zone_cases = [
                ('America/New_York', '12/31/2025 19:30', '%m/%d/%Y'),
                ('Europe/Berlin', '01.01.2026 01:30', '%d.%m.%Y'),
                ('Asia/Tokyo', '2026/01/01 09:30', '%Y/%m/%d'),
            ]

            def wait_for_zone(zone, expected, date_format):
                page.wait_for_function('zone => timezonePicker.active === zone', arg=zone, timeout=10000)
                actual = page.evaluate('''() => previewSensorTimestamp("2026-01-01T00:30:00Z", "datetime")''')
                assert actual == expected, (zone, actual)
                local = datetime.datetime.fromisoformat(page.evaluate('editor.settings.time.local_time'))
                assert page.locator('#settingsLocalTime').inner_text().startswith(local.strftime(date_format))
                assert page.locator('#settingsActiveTimezone').inner_text() == zone
                assert page.locator('#settingsTimezone').input_value() == zone

            def change_location_on_device(zone):
                with server.local_store.lock:
                    settings.setdefault('time', {})['timezone'] = zone

            open_display()
            assert page.locator('#settingsClockStyle').input_value() == 'analog'
            page.evaluate('setActiveSettingsSection("settingsUiSection")')
            for zone, expected, date_format in zone_cases:
                picker.fill(zone)
                with page.expect_response(lambda r: r.url.endswith('/api/settings') and r.request.method == 'PUT') as response:
                    page.get_by_role('option', name=zone, exact=True).click()
                assert response.value.ok
                clock = response.value.json()
                assert clock['active_timezone'] == zone
                wait_for_zone(zone, expected, date_format)
                assert picker.input_value() == zone
            open_display()
            assert page.evaluate('editor.settings.time.active_timezone') == 'Asia/Tokyo'
            checks.append('Timezone picker: saved and reloaded, panel-local midnight and date on a Honolulu browser')
            page.evaluate('setActiveSettingsSection("settingsUiSection"); window.timezoneTestPage = true')
            saves = []

            def track_save(req):
                if req.method == 'PUT' and req.url.endswith('/api/settings'):
                    saves.append(req.url)

            page.on('request', track_save)
            for case in zone_cases:
                change_location_on_device(case[0])
                wait_for_zone(*case)
                assert picker.input_value() == case[0]

            picker.fill('berl')
            picker.press('ArrowDown')
            picker.evaluate('input => input.setSelectionRange(1, 3)')
            option = page.get_by_role('option', name='Europe/Berlin', exact=True)
            active_option = picker.get_attribute('aria-activedescendant')
            assert option.get_attribute('aria-selected') == 'false'
            change_location_on_device('America/New_York')
            change_location_on_device('Europe/Berlin')
            wait_for_zone(*zone_cases[1])
            assert picker.input_value() == 'berl' and picker.get_attribute('aria-activedescendant') == active_option
            assert picker.evaluate('input => [input === document.activeElement, input.selectionStart, input.selectionEnd]') == [True, 1, 3]
            assert option.get_attribute('aria-selected') == 'true'
            picker.press('Escape')
            assert picker.input_value() == 'Europe/Berlin'
            assert page.evaluate('window.timezoneTestPage') and not saves
            page.remove_listener('request', track_save)
            checks.append('Native zone changes: New York, Berlin and Tokyo in the same editor page; rapid changes and open search')
            open_display()
            for style in ['flip', 'digital', 'analog', 'flip']:
                payload = select('#settingsClockStyle', style)
                assert payload['hardware']['clock']['style'] == style and payload['reboot'] is False
                open_display()
                assert page.locator('#settingsClockStyle').input_value() == style
                for field in ['Seconds', 'Date']:
                    assert page.locator(f'#settingsClock{field}Row').is_visible() == (style != 'flip')
                    assert not page.locator(f'#settingsClock{field}').is_checked()
                assert not page.locator('#settingsClock24h').is_checked()
                select('#settingsSaverMode', 'disabled')
                assert not page.locator('#settingsClockFields').is_visible()
                payload = select('#settingsSaverMode', 'clock')
                assert page.locator('#settingsClockStyle').input_value() == style
                clock = payload['hardware']['clock']
                assert clock['style'] == style and clock['analog'] == (style == 'analog')
                for key, value in {'format_24h': False, 'show_date': False, 'show_seconds': False,
                                   'brightness': 17, 'off_enabled': True, 'off_seconds': 600, 'start_seconds': 60}.items():
                    assert clock[key] == value, (style, key, clock[key])
                checks.append(f'{style}: event, save, reload, disable/enable, retained options')
            for language in ['en', 'de', 'es', 'fr', 'pl', 'zh-cn', 'zh-tw']:
                settings['ui']['language'] = language
                open_display()
                labels = page.locator('#settingsClockStyle option').all_text_contents()
                expected = page.evaluate('''language => ['digital', 'analog', 'flip'].map(
                    style => WEB_I18N_BUILTIN[language]['settings.screen.clock_' + style])''', language)
                assert len(labels) == 3 and all(expected) and labels == expected, (language, labels, expected)
                for width in [1440, 390]:
                    page.set_viewport_size({'width': width, 'height': 1000})
                    page.locator('#settingsClockStyle').scroll_into_view_if_needed()
                    issues = page.evaluate('''() => {
                        const section = document.getElementById('settingsClockStyle').closest('section');
                        const issues = [];
                        for (const input of section.querySelectorAll('input, select, button')) {
                            if (!input.offsetWidth || input.closest('.visually-hidden')) continue;
                            const box = input.getBoundingClientRect();
                            const group = input.closest('.form')?.getBoundingClientRect();
                            if (group && (box.left < group.left-2 || box.right > group.right+2)) issues.push(input.id);
                        }
                        if (document.documentElement.scrollWidth > innerWidth+2) issues.push('page overflow');
                        return issues;
                    }''')
                    assert not issues, (language, width, issues)
                    if language in ['en', 'zh-cn']:
                        page.screenshot(path=str(out / f'editor-{language}-{width}.png'))
                checks.append(f'{language}: three translated choices, 1440/390 bounds')
            assert server.local_store.responses['/api/layout'] == original_layout
            assert not errors, errors
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
    (out / 'editor-result.json').write_text(json.dumps({'passed': checks, 'layout_unchanged': True}, indent=2), encoding='utf-8')
    print('Clock editor: PASS (live timezone sync, search, persistence, clock options, 7 languages, wide/narrow bounds)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='betta-clock-editor-') as tmp:
        check(args.browser, args.output or Path(tmp))


if __name__ == '__main__':
    main()
