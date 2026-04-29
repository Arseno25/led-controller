# Build, Flash, and Release

Firmware ini target-aware, tetapi ESP-IDF tetap membutuhkan binary berbeda
untuk setiap chip target. Satu source tree bisa membangun semua target, namun
`esp32`, `esp32s3`, `esp32c3`, dan target lain tidak bisa memakai satu `.bin`
yang sama.

## Supported Targets

| Target | Build folder | sdkconfig |
| --- | --- | --- |
| `esp32` | `build-esp32` | `sdkconfig.esp32` |
| `esp32s2` | `build-esp32s2` | `sdkconfig.esp32s2` |
| `esp32s3` | `build-esp32s3` | `sdkconfig.esp32s3` |
| `esp32c3` | `build-esp32c3` | `sdkconfig.esp32c3` |
| `esp32c6` | `build-esp32c6` | `sdkconfig.esp32c6` |

The local helper keeps these folders separated so changing one target does not
break another build output.

## Standard ESP-IDF Commands

Use this when working inside an exported ESP-IDF terminal.

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

For ESP32-S3:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

This standard workflow uses the default `build/` folder and `sdkconfig`.

## Cross-Platform Helper

Universal helper:

```bash
python tools/firmware.py build --target esp32
python tools/firmware.py flash --target esp32 --port COM5 --monitor
```

Linux/macOS:

```bash
python3 tools/firmware.py build --target esp32s3
python3 tools/firmware.py flash --target esp32s3 --port /dev/ttyUSB0 --monitor
```

Supported commands:

| Command | Purpose |
| --- | --- |
| `build` | Build selected target. |
| `flash` | Flash selected target. |
| `monitor` | Open ESP-IDF monitor. |
| `erase` | Erase flash. |
| `menuconfig` | Open menuconfig for selected target. |
| `clean` | Clean selected target build folder. |

Optional default target:

```bash
export LED_CONTROLLER_TARGET=esp32s3
python tools/firmware.py build
```

On Windows, the helper can use an exported ESP-IDF terminal or a standard
Espressif install at `C:\Espressif`.

## Local Packaging

After a target build succeeds, create the same ZIP format used by CI:

```bash
python tools/package_release.py --target esp32s3 --version test --output-dir dist
```

Output example:

```text
dist/led-controller-test-esp32s3.zip
```

ZIP contents:

- `led-controller.bin`
- `bootloader/bootloader.bin`
- `partition_table/partition-table.bin`
- `flash_args`
- `flasher_args.json`
- `firmware-manifest.json`
- `README-FLASH.md`
- `flash.sh`
- `flash.bat`
- `sdkconfig.<target>`

## GitHub Release CI/CD

Workflow:

`.github/workflows/release-firmware.yml`

Triggers:

- GitHub Release published
- manual `workflow_dispatch`

For each release, GitHub Actions builds:

- `led-controller-<tag>-esp32.zip`
- `led-controller-<tag>-esp32s2.zip`
- `led-controller-<tag>-esp32s3.zip`
- `led-controller-<tag>-esp32c3.zip`
- `led-controller-<tag>-esp32c6.zip`

The `publish` job attaches every ZIP to the GitHub Release. Users only need to
download the ZIP matching their board target.

The workflow also updates `CHANGELOG.md` automatically. It collects commits
between the previous reachable tag and the new release tag, groups them by
commit prefix, commits the updated changelog back to the default branch, and
attaches `CHANGELOG.md` to the GitHub Release.

Recommended commit prefixes:

| Prefix | Changelog group |
| --- | --- |
| `feat:` | Added |
| `fix:` | Fixed |
| `perf:` | Performance |
| `refactor:` | Changed |
| `docs:` | Documentation |
| `ci:` / `build:` | Build and CI |
| `test:` / `chore:` | Maintenance |

Use `feat!:` or `fix!:` for breaking changes.

## Flashing a Release ZIP

Extract the ZIP matching the board, then run one of these commands inside the
extracted folder.

Windows:

```bat
flash.bat COM5
```

Linux/macOS:

```bash
chmod +x flash.sh
./flash.sh /dev/ttyUSB0
```

Manual esptool:

```bash
python -m esptool --chip esp32s3 -p /dev/ttyUSB0 -b 460800 --before default_reset --after hard_reset write_flash @flash_args
```

Change `--chip` and port to match the target and host OS.

## Default Pins

Pin defaults are documented in `docs/hardware.md` and defined in:

`components/config_manager/include/board_profile.h`

Do not reuse ESP32 classic wiring blindly on ESP32-S3/C3/C6 boards. Some GPIOs
do not exist or are reserved on different chip families.

## Build Verification

The current project has been locally verified for:

- `esp32`
- `esp32s2`
- `esp32s3`
- `esp32c3`
- `esp32c6`

The app partition is `1536K`, leaving enough headroom for the current firmware
on the supported targets.
