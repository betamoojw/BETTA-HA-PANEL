<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Cameras

**Settings → Cameras** manages the Waveshare LCD-X built-in camera and up to four
saved IP or Home Assistant cameras. Image output and MJPEG streaming both default
off. Enabling room-light or approach sensing in **Settings → Display** does not
enable pictures.

## Built-in camera

On the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 with OV5647:

1. Enable **camera image output** and save the camera settings.
2. Choose **Preview (30s)** to view pictures in the browser. **Stop preview**,
   leaving the page or hiding the browser stops the preview. Editing a control
   does not extend the 30-second deadline.
3. Optionally enable **Allow MJPEG streaming** and save. The address shown on this
   page supplies a live MJPEG response while a client is connected.
4. Use **Add built-in camera tile**, then save the layout, to show it on the panel.
   Tap a live tile to open its full-screen view.

Choose native or half-size output, JPEG quality 10–95, and horizontal or vertical
mirroring. Built-in pictures automatically follow the applied display rotation;
the sizes are 800 × 640 / 400 × 320, or 640 × 800 / 320 × 400 after a quarter turn.
Snapshots, streams, browser previews and panel views share the same orientation.
The LCD-X's 90-degree display setting is the upright raw-camera reference.
Image adjustments include brightness, contrast, saturation, hue, red
and blue balance, sharpening, noise reduction, shadows and highlights. These are
software adjustments to the picture; they do not recalibrate ambient-light sensing.
Save changes before starting a preview. Live output is capped at two images per
second; actual freshness depends on capture, encoding and the network.

Pictures are held in RAM, without a recording archive. A connected viewer receives
the images you enable. Disabling image output stops local picture requests and
clears old built-in-camera pictures on the panel. Sensor-only automation remains
available. The panel's ordinary editor authentication also protects image routes.

## Motion detection

**Use advanced motion detection** replaces the simple 1–10 sensitivity while
enabled. It can sense movement with picture output disabled. The separate Display
switches still decide whether movement wakes the screen, including at night.

- **Motion threshold:** 1–64; lower detects smaller changes.
- **Minimum changed area:** percentage of selected grid cells that must change.
- **Minimum duration:** how long movement must persist before a trigger.
- **Pause between triggers:** avoids repeated wake signals.
- **Startup grace period:** lets exposure settle after enabling or changing detection.
- **Ignore whole-room lighting changes:** suppresses broad changes in one direction.

Up to four rectangular zones select where motion is measured. Enter their position
and size as percentages, or drag rectangles on a running built-in-camera preview.
No zones selects the whole frame. Saved coordinates use the mirrored camera frame;
the preview rotates zone outlines and maps new selections back to those coordinates.
Changing display rotation therefore keeps the same scene areas selected. Numeric
zone coordinates refer to the camera's unrotated frame. The status shows movement level, changed area, trigger
count and each zone's readings.

**Test motion (30s)** runs sensing while the screen is awake; it does not enable
pictures. The panel owns the timeout, so the test expires if the browser closes.
Saved advanced detection continues after the temporary test ends. Motion detection
needs visible light; enabling night wake does not provide infrared night vision.

## Saved cameras

Add up to four sources, give each a stable ID and choose:

- **Home Assistant:** a `camera.*` entity that provides JPEG snapshots.
- **HTTP / HTTPS JPEG:** a direct JPEG snapshot URL, with optional username and
  password for Basic authentication. HTTPS uses the firmware's trusted certificates.

Enable the source, select a refresh interval and save. Preview it or add a tile to
the selected page. Saved sources work without enabling built-in image output; IP
cameras do not require a Home Assistant connection. Existing HA Camera tiles can
still use their entity's MJPEG stream. RTSP, H.264/H.265 and audio are unsupported.

Leave a saved password field untouched to retain it. Editing it to blank removes
it. Changing the URL, username or source type clears the old password unless you
enter a replacement. Credentials are sent only to that source; redirects are
rejected. HTTP Basic credentials are not encrypted over plain HTTP.

Camera settings have their own **Save**, **Reload** and **Reset this page to
defaults** controls. Reset is a draft until saved; it covers image, motion and
saved-source settings. Full panel backups include camera settings and source IDs,
but omit passwords. Re-enter passwords after restoring to a different panel.
Older backups leave the camera configuration unchanged.

## Validation

The original LCD-X light/approach/night-wake automation has a bounded device trial
record in [Waveshare 10.1-inch](hardware/panel10.md). The expanded image, motion-zone
and saved-source features have local automated checks. A bounded LCD-X trial also
passed JPEG/MJPEG decoding, both sizes, image controls and device test expiry;
the owner confirmed preview quality and automatic stop at display rotation 90.
The 2026-10-28 rotation build passed four-angle JPEG decoding at both sizes,
MJPEG decoding and live browser preview resizing; rotation did not extend the
30-second preview. The owner confirmed upright, correctly proportioned pictures
at all four display angles. At display setting 90, the owner also confirmed
snapshot/live tiles, full-screen picture, touch and two open/close cycles.
Leaving the page stopped capture; returning resumed it. Motion-zone coexistence
still needs its device checks.
External-source acceptance is deferred because no external camera was available.
The local editor uses synthetic settings and explicitly labels its simulation;
it cannot open a camera or stream pictures.

Implementation and reference comparison: [Local camera](development/local-camera.md).
