<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk | Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Release notes architecture

### Under the hood

The sections below matter to developers and maintainers: toolchain, memory, test coverage and the flash layout.

#### Toolchain and dependencies

The fork moves the firmware build from **ESP-IDF 5.5.2 to 6.1** and the UI
from **LVGL 9.5.0 to 9.6.0**. This updates the framework and rendering stack,
alongside the individual components:

| Component | Upstream v0.8.2 | Fork v0.9.0 |
| --- | --- | --- |
| **ESP-IDF** | **5.5.2** | **6.1** |
| **LVGL** | **9.5.0** | **9.6.0** (registry package `9.6.0~1`) |
| ESP-Hosted | 2.11.7 | 2.12.13 |
| Wi-Fi Remote | 1.5.1 | 1.6.5 |
| WebSocket client | 1.7.0 | 1.8.0 |
| LittleFS | 1.21.1 | 1.23.0 |

The upstream column comes from the README and committed lock file at
`420df764`; it describes the existing Waveshare build, not a previous JC or
Tab5 release. Shared component versions are pinned across profiles, and the
LVGL/SDL simulator uses the same 9.6 rendering sources.

ESP-IDF 6 integration includes explicit managed cJSON, MQTT and USB components,
updated component requirements and LCD API names in the Waveshare BSPs. Required
patches are reapplied automatically during CMake configuration so a component
refetch retains them. Use [the ESP-IDF 6.1 build instructions](development/build.md#set-up).

??? info "ESP-Hosted and C6 firmware"

    P4 builds use ESP-Hosted 2.12.13 and its matching C6 adapter firmware. The
    dependency includes locked RPC response tables, correct failure reporting
    and receive-path handling for slow RPCs. A required C6 update runs once and
    restarts the panel; with OTA rollback enabled it waits until the P4 app has
    passed validation. The newly added JC and Tab5 profiles use the same stack.

??? info "Dependencies for the new camera and audio profiles"

    JC support uses `esp_codec_dev` 1.6.2. JC and Tab5 cameras use `esp_video`
    2.3.0, `esp_cam_sensor` 2.3.0 and `esp_ipa` 2.2.0. The local sensor component
    includes the SC202CS/SC2356 driver from the same upstream release for Tab5.
    These are dependencies of new fork profiles. Newer video/IPA combinations
    require instructions unsupported by the targeted P4 v1.x silicon; see
    [the component version caps](development/components.md#vendored-camera-component-and-the-p4-silicon-cap).

#### Performance and memory

??? info "LVGL allocation and measured fork headroom"

    All P4 profiles allocate LVGL objects from PSRAM first, reserving scarce
    internal RAM for Wi-Fi/SDIO, TLS and task stacks. The measurements below
    were recorded on **2026-10-01 with fork development builds**. They are
    snapshots of those test builds, rather than upstream-versus-fork benchmarks.

    | Display device | Free internal heap after boot | Largest free internal block | Lowest free internal heap in stress runs |
    | --- | --- | --- | --- |
    | Waveshare `panel4` | 329 KB | 152 KB | 165–226 KB across separate runs |
    | Newly added Guition `paneljc` | 321 KB | 163.8 KB | 184 KB |

    Other profiles use the same allocation policy without their own confirmed
    measurements here. Earlier/later fork comparisons and workload details
    remain in [Memory and heap](development/learnings.md).

- **Layout initialization:** the shared state pass visits each widget once,
  avoiding repeated per-entity scans. A fork development test on the newly
  supported paneljc rendered 168 widgets in about 2.2 s and 224 in 3.6 s.
  See [layout measurements](development/learnings.md) for test conditions.

#### Development and verification

??? info "Documentation, examples and GitHub Pages"

    **Documentation:** the site is organised for two audiences: a feature tour, a Pages and layouts guide and a
      grouped widget library with thumbnails for Home Assistant users, and the build, hardware and device notes for
      developers; the release notes list user-facing changes before the developer sections.
      The README now highlights selected current dashboard and
      editor examples, with detailed installation, feature and editor guides in `docs/`.
      The hardware overview includes M5Stack Tab5 in one comparison ordered by display size,
      with audio and camera functions and their validation status.
      Device guides share manufacturer references and labelled illustrations with a short flashing-connector instruction. Public demo
      screenshots come from the LVGL simulator and the current editor against an offline
      API. `scripts/editor_local.sh` serves the current editor with synthetic data and local saves, without flashing. Zensical builds the documentation with uv and locked dependencies; CI validates
      pull requests and publishes `main` to GitHub Pages in the maintainer's fork.
      The unused upstream screenshots are retired from the published tree; the
      unchanged original BETTA OS logo lives in `docs/img/brand/` with attribution.
      Direct `.html` navigation supports opening the built pages from the filesystem;
      the wider content area and scrollable gallery tables keep widget previews readable.
      Hardware documentation separates device selection and display device guides from CPU/memory,
      partition/storage and peripheral references. Display controls have their own guide;
      Appearance compares the same dashboard side by side in all four styles.
      Each widget has a guide under Get started, followed by a [Top bar](top-bar.md) guide
      for date/weather/stock slots, the clock and connection detail cards, with cropped
      4-, 5- and 10-inch examples explaining the available space, including Date + Weather + Stock
      on wide displays with a demo VWCE.DE ETF quote. Gallery previews preserve relative
      pixel sizes, detailed widgets use wider comparisons, and documentation images
      open in a local enlarged viewer with Close/Escape controls.
      Installation documents published GitHub images and source builds as separate
      paths, including display device/revision selection, factory versus OTA images, adapter
      firmware and first-run setup. Published images cover Waveshare 4-inch and Guition original 10-inch.
      Setup examples show the real boot splash with demo AP credentials and the WebUI's
      Wi-Fi/Home Assistant stages. The Select example is wider and includes its option
      picker; Recorder history has a user guide and widget-level popup comparisons.
      Notifications include finished overlay previews and separate MQTT and REST setup
      paths. Gallery generators write Markdown and images; Zensical produces the HTML pages,
      replacing the separate standalone gallery exports. The documentation search indexes pages and sections and works from
      `file://` with Zensical's offline mode and a locally bundled worker shim.
      README and site invite forks, personal adaptations, issues and pull requests,
      with a short [Contributing](contributing.md) guide and links to this fork.
      Appearance and notification user guides link to separate implementation
      references. Widget guides use readable option tables; installation, backups,
      night shutoff and the local/Recorder graph sources have explicit instructions.

- Variant build wrappers, pinned shared dependencies, host tests, editor tests,
  source checks and SDL layout checks support panel4 and paneljc; CI installs pinned FontTools, Brotli and Pillow.
  The simulator renders firmware widgets and page chrome; the
  [dashboard examples](features.md) and [10-inch gallery](gallery-sim-paneljc.md) use public synthetic data.
  Household captures stay local; vendored camera build files carry Apache-2.0 modification notices. See [development](development.md) for commands and limits.

#### Why this fork changes flash and boot behaviour

??? info "OTA placement, crash evidence and bootloader recovery"

    - **Bootable OTA locations:** upstream v0.8.2's 32 MiB table places the second
      OTA slot above the ESP32-P4's current 16 MiB quad-flash cache mapping range.
      The fork uses two app slots below that boundary so an update can boot from
      either location. The current fork uses two 6.5 MiB slots on all P4 profiles,
      including the newly introduced 16 MiB JC and Tab5 targets. This corrects the
      inherited 32 MiB mapping issue and defines storage for the new profiles;
      LittleFS data can still be accessed above 16 MiB through the SPI flash API.
    - **Crash evidence after restart:** dedicated panic-dump partitions retain
      the failing task and execution state so a crash can be investigated after
      the panel reboots. They are separate from the eight-entry restart history
      in NVS and the running text log in PSRAM. See [coredumps](coredump.md).
    - **Recovery from a failed update:** with the rollback-capable bootloader,
      the new app must pass its uptime, HTTP and network checks before it is
      accepted. A reset before validation returns to the preceding firmware,
      reducing the need for USB
      recovery after a bad OTA update. Installing this bootloader needs USB once.
      This is OTA recovery, not ESP Secure Boot or signed-image enforcement.
