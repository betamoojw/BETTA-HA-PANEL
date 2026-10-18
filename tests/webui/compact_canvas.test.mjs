// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadEditor } from './load_editor.mjs';

test('complex widgets can be placed and resized inside the S3 canvas', () => {
  const { fn } = loadEditor({ width: 480, height: 360 });
  for (const type of ['wallbox_evcc', 'bambu_printer', 'weather_3day', 'solar_forecast',
    'media_player', 'car_widget', 'energy_timeline', 'vacuum_tile', 'todo_list', 'sonos_favorites']) {
    const limits = fn('widgetSizeLimits')(type);
    assert.ok(limits.minW <= 456 && limits.minH <= 336, type + ' fits with a margin');
    for (const input of [{ x: 12, y: 12, w: limits.minW, h: limits.minH },
      { x: 650, y: 400, w: 1280, h: 680 }]) {
      const rect = fn('clampRectToCanvas')(input, type);
      assert.ok(rect.x >= 0 && rect.y >= 0, type);
      assert.ok(rect.x + rect.w <= 480 && rect.y + rect.h <= 360, type);
      assert.ok(rect.w >= limits.minW && rect.h >= limits.minH, type);
    }
  }
  assert.equal(fn('widgetSizeLimits')('wallbox_evcc').minH, 320);
});

test('larger boards retain the existing wallbox minimum', () => {
  const { fn } = loadEditor({ width: 1280, height: 680 });
  assert.equal(fn('widgetSizeLimits')('wallbox_evcc').minH, 360);
});
