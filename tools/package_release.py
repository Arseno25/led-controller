#!/usr/bin/env python3
"""Package an ESP-IDF build into a board-specific release ZIP."""

from __future__ import annotations

import argparse
import json
import os
import zipfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROJECT_NAME = "led-controller"
SUPPORTED_TARGETS = ("esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6")


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def require_file(path: Path) -> Path:
    if not path.exists():
        raise FileNotFoundError(f"Required build output missing: {path}")
    return path


def package_readme(target: str, version: str) -> str:
    return f"""# {PROJECT_NAME} {version} - {target}

This package contains firmware binaries for `{target}` only.
Do not flash this ZIP to a different ESP32 family target.

## Files

- `led-controller.bin` - main application firmware
- `bootloader/bootloader.bin` - ESP-IDF bootloader
- `partition_table/partition-table.bin` - partition table
- `flash_args` - ESP-IDF flash offsets and flash settings
- `flasher_args.json` - structured flash metadata from ESP-IDF

## Flash

Install ESP-IDF or esptool.py, connect the board, then run from this folder:

```bash
python -m esptool --chip {target} -b 460800 --before default_reset --after hard_reset write_flash @flash_args
```

Windows:

```bat
flash.bat COM5
```

Linux/macOS:

```bash
chmod +x flash.sh
./flash.sh /dev/ttyUSB0
```

After flashing, the default AP is `PixelController-Setup` with password `12345678`.
Open `http://192.168.4.1` after connecting to the AP.
"""


def flash_sh(target: str) -> str:
    return f"""#!/usr/bin/env sh
set -eu

PORT="${{1:-}}"
if [ -z "$PORT" ]; then
  echo "Usage: ./flash.sh /dev/ttyUSB0"
  exit 1
fi

python -m esptool --chip {target} -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @flash_args
"""


def flash_bat(target: str) -> str:
    return f"""@echo off
set PORT=%1
if "%PORT%"=="" (
  echo Usage: flash.bat COM5
  exit /b 1
)

python -m esptool --chip {target} -p %PORT% -b 460800 --before default_reset --after hard_reset write_flash @flash_args
"""


def build_manifest(target: str, version: str, build_dir: Path) -> dict:
    flasher_args = read_json(build_dir / "flasher_args.json")
    project_description = read_json(build_dir / "project_description.json")

    return {
        "project": PROJECT_NAME,
        "version": version,
        "target": target,
        "idf_target": project_description.get("target", target),
        "project_version": project_description.get("project_version"),
        "esp_idf": project_description.get("idf_ver")
        or project_description.get("idf_version")
        or project_description.get("git_revision"),
        "flash_settings": flasher_args.get("flash_settings", {}),
        "flash_files": flasher_args.get("flash_files", {}),
        "flash_command": (
            f"python -m esptool --chip {target} -b 460800 "
            "--before default_reset --after hard_reset write_flash @flash_args"
        ),
    }


def add_file(zip_handle: zipfile.ZipFile, path: Path, archive_root: str, archive_name: str) -> None:
    zip_handle.write(path, f"{archive_root}/{archive_name}")


def package_target(target: str, version: str, output_dir: Path) -> Path:
    build_dir = PROJECT_ROOT / f"build-{target}"
    output_dir.mkdir(parents=True, exist_ok=True)

    app_bin = require_file(build_dir / "led-controller.bin")
    bootloader_bin = require_file(build_dir / "bootloader" / "bootloader.bin")
    partition_bin = require_file(build_dir / "partition_table" / "partition-table.bin")
    flash_args = require_file(build_dir / "flash_args")
    flasher_args = require_file(build_dir / "flasher_args.json")

    package_name = f"{PROJECT_NAME}-{version}-{target}"
    zip_path = output_dir / f"{package_name}.zip"

    manifest = build_manifest(target, version, build_dir)
    readme = package_readme(target, version)

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        add_file(zf, app_bin, package_name, "led-controller.bin")
        add_file(zf, bootloader_bin, package_name, "bootloader/bootloader.bin")
        add_file(zf, partition_bin, package_name, "partition_table/partition-table.bin")
        add_file(zf, flash_args, package_name, "flash_args")
        add_file(zf, flasher_args, package_name, "flasher_args.json")

        sdkconfig = PROJECT_ROOT / f"sdkconfig.{target}"
        if sdkconfig.exists():
            add_file(zf, sdkconfig, package_name, f"sdkconfig.{target}")

        zf.writestr(f"{package_name}/firmware-manifest.json", json.dumps(manifest, indent=2) + "\n")
        zf.writestr(f"{package_name}/README-FLASH.md", readme)
        zf.writestr(f"{package_name}/flash.sh", flash_sh(target))
        zf.writestr(f"{package_name}/flash.bat", flash_bat(target))

    print(zip_path)
    return zip_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a board-specific firmware release ZIP.")
    parser.add_argument("-t", "--target", choices=SUPPORTED_TARGETS, required=True)
    parser.add_argument(
        "-v",
        "--version",
        default=os.environ.get("RELEASE_VERSION", "dev"),
        help="Release version/tag used in the ZIP filename.",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        default="dist",
        help="Output directory for ZIP files.",
    )
    args = parser.parse_args()

    package_target(args.target, args.version, PROJECT_ROOT / args.output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
