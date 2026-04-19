# Ideas

## Energy dashboard widget

Status: idea captured, implementation deferred.

Create a Home Assistant style energy distribution dashboard for the panel.

Core behavior:
- Show energy sources, storage, consumers, and grid as circular nodes connected by colored flow lines.
- Animate small dots on each line in the direction the energy is currently flowing.
- Use the line color for the moving dots.
- Increase dot speed as the current power flow increases.
- Hide or slow the animation when a flow is zero or unavailable.
- Keep the visual language close to Home Assistant energy cards: dark background, colored rings, compact labels, icons, and live values.

Likely nodes:
- Home / current load
- Grid import and export
- Solar production
- Battery charge / discharge and battery percentage
- Optional low-carbon / gas / EV / water heater nodes if configured

Possible data model:
- One full-size widget type, for example `energy_dashboard`.
- Main entity could represent home consumption.
- Additional configured entities could provide solar, grid import/export, battery power, battery state of charge, EV, gas, or heat pump values.
- Flow direction should be derived from signed power values where possible.
- Use W/kW for live flow and Wh/kWh for daily energy totals where available.

Implementation notes for later:
- Implement as a custom LVGL widget rather than multiple separate tiles.
- Draw connection lines on a canvas or lightweight custom draw layer.
- Use an LVGL timer for animation ticks.
- Compute dot offsets from elapsed time so animation stays smooth without storing many objects.
- Scale speed non-linearly so small flows remain visible and large flows feel faster without becoming noisy.
- Keep CPU and memory use low enough for the panel by limiting dot count and redraw area.
- Add validation support for optional configured entity IDs before exposing it in the web layout editor.

Reference style:
- Home Assistant energy distribution card with animated flow dots.
