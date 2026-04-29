# ESP32 LED Controller

ESP-IDF firmware untuk LED controller bergaya SP107E dengan Web UI lokal,
mode basic/normal, reactive audio dari INMP441, matrix mode, dan display bulat
GC9A01.

## Features

- ESP32 family target-aware build: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`
- Addressable LED strip: WS2812B, WS2811, SK6812 RGB/RGBW
- Normal/basic strip effects
- Reactive strip effects from INMP441 I2S microphone
- Matrix and reactive matrix effects
- GC9A01 240 x 240 display for boot animation, status, FPS/WiFi, and visualizer
- Web UI hosted directly on ESP32 SoftAP
- JSON HTTP API
- Config persistence through ESP-IDF NVS
- GitHub Actions release builds with ZIP per target board
- Automatic `CHANGELOG.md` generation from release commits

## Documentation

| Document | Purpose |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Firmware architecture, tasks, data flow, modules. |
| [docs/hardware.md](docs/hardware.md) | Wiring, default pins, power notes, target pin maps. |
| [docs/usage.md](docs/usage.md) | Web UI usage, modes, display, audio, troubleshooting. |
| [docs/api.md](docs/api.md) | HTTP API endpoints and payload examples. |
| [docs/target-builds.md](docs/target-builds.md) | Build, flash, package, and GitHub release workflow. |
| [CHANGELOG.md](CHANGELOG.md) | Generated release changelog. |

## Quick Start

Standard ESP-IDF flow:

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

For ESP32-S3:

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Cross-platform helper:

```bash
python tools/firmware.py build --target esp32s3
python tools/firmware.py flash --target esp32s3 --port <PORT> --monitor
```

Use `python3` on Linux/macOS if `python` is not available.

## Default WiFi

After boot, connect to:

| Field | Value |
| --- | --- |
| SSID | `PixelController-Setup` |
| Password | `12345678` |
| Web UI | `http://192.168.4.1` |

## Project Layout

```text
led-controller/
|-- .github/workflows/          # CI/CD release firmware builds
|-- components/
|   |-- app_controller/         # runtime orchestrator and tasks
|   |-- config_manager/         # defaults, validation, NVS
|   |-- led_driver/             # LED RMT abstraction
|   |-- web_server/             # HTTP API and embedded Web UI
|   |-- wifi_service/           # SoftAP service
|   |-- audio_input/            # INMP441 I2S input
|   |-- audio_processor/        # audio features and spectrum
|   |-- reactive_engine/        # audio task lifecycle
|   |-- reactive_renderer/      # strip audio visualizers
|   |-- matrix_engine/          # matrix mapping/effects
|   |-- reactive_matrix_renderer/
|   |-- display_manager/        # GC9A01 display integration
|   |-- effect_registry/        # effect metadata
|   |-- palette_manager/
|   |-- random_effect_manager/
|   `-- system_monitor/
|-- docs/
|-- main/
|-- src/display/                # display rendering implementation
|-- tools/
|   |-- firmware.py             # build/flash helper
|   `-- package_release.py      # board ZIP packager
|-- partitions.csv
|-- sdkconfig.defaults
`-- CMakeLists.txt
```

## Release Artifacts

Publishing a GitHub Release builds and attaches one ZIP per target:

```text
led-controller-<tag>-esp32.zip
led-controller-<tag>-esp32s2.zip
led-controller-<tag>-esp32s3.zip
led-controller-<tag>-esp32c3.zip
led-controller-<tag>-esp32c6.zip
```

Each ZIP includes app binary, bootloader, partition table, flash args, and
simple `flash.sh` / `flash.bat` scripts.

## Hardware Notes

Start from [docs/hardware.md](docs/hardware.md). The most important points:

- Use external LED power.
- Connect ESP32 GND and LED power supply GND together.
- Use the correct target-specific pin map.
- Do not flash an `esp32` binary to an `esp32s3` board.

## License

This project is licensed under the [MIT License](LICENSE).
