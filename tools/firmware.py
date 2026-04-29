#!/usr/bin/env python3
"""Cross-platform ESP-IDF build and flash helper."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SUPPORTED_TARGETS = ("esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6")


def find_idf_path() -> Path | None:
    idf_path = os.environ.get("IDF_PATH")
    if idf_path and (Path(idf_path) / "tools" / "idf.py").exists():
        return Path(idf_path)

    candidates = [
        Path.home() / "esp" / "esp-idf",
        Path.home() / "esp-idf",
        Path("/opt/esp/idf"),
        Path("/opt/esp/esp-idf"),
    ]

    if os.name == "nt":
        frameworks = Path("C:/Espressif/frameworks")
        if frameworks.exists():
            candidates.extend(sorted(frameworks.glob("esp-idf-*"), reverse=True))

    for candidate in candidates:
        if (candidate / "tools" / "idf.py").exists():
            return candidate

    return None


def find_idf_python() -> str:
    python_env = os.environ.get("IDF_PYTHON_ENV_PATH")
    if python_env:
        executable = Path(python_env) / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        if executable.exists():
            return str(executable)

    if os.name == "nt":
        env_root = Path("C:/Espressif/python_env")
        if env_root.exists():
            candidates = sorted(env_root.glob("idf*_env/Scripts/python.exe"), reverse=True)
            if candidates:
                return str(candidates[0])

    return sys.executable


def load_idf_environment(idf_path: Path, python_executable: str, env: dict[str, str]) -> dict[str, str]:
    export_script = idf_path / "tools" / "idf_tools.py"
    if not export_script.exists():
        return env

    export_env = env.copy()
    export_env["IDF_PATH"] = str(idf_path)
    completed = subprocess.run(
        [python_executable, str(export_script), "export", "--format", "key-value"],
        cwd=PROJECT_ROOT,
        env=export_env,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        return export_env

    original_path = export_env.get("PATH", "")
    for raw_line in completed.stdout.splitlines():
        line = raw_line.strip()
        if not line or "=" not in line:
            continue

        key, value = line.split("=", 1)
        if not key.replace("_", "").isalnum():
            continue

        value = value.replace("%PATH%", original_path).replace("$PATH", original_path)
        export_env[key] = value

    export_env.setdefault("IDF_PATH", str(idf_path))
    return export_env


def idf_command() -> tuple[list[str], dict[str, str]]:
    env = os.environ.copy()
    idf_path = find_idf_path()
    if idf_path:
        python_executable = find_idf_python()
        env = load_idf_environment(idf_path, python_executable, env)
        env.setdefault("IDF_PATH", str(idf_path))
        return [python_executable, str(idf_path / "tools" / "idf.py")], env

    if shutil.which("idf.py"):
        return ["idf.py"], env

    message = (
        "ESP-IDF tidak ditemukan. Jalankan export/install environment ESP-IDF dulu, "
        "atau set IDF_PATH ke folder esp-idf."
    )
    raise RuntimeError(message)



def target_args(target: str) -> list[str]:
    return [
        "-B",
        f"build-{target}",
        "-D",
        f"SDKCONFIG=sdkconfig.{target}",
    ]


def sdkconfig_has_target(target: str) -> bool:
    sdkconfig = PROJECT_ROOT / f"sdkconfig.{target}"
    if not sdkconfig.exists():
        return False

    expected = f'CONFIG_IDF_TARGET="{target}"'
    try:
        return expected in sdkconfig.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return False


def maybe_set_target(command: list[str], target: str, skip_set_target: bool) -> None:
    if not skip_set_target and not sdkconfig_has_target(target):
        command += ["set-target", target]


def run_idf(args: list[str]) -> int:
    try:
        command_base, env = idf_command()
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 127

    command = command_base + args
    print(" ".join(command), flush=True)
    try:
        completed = subprocess.run(command, cwd=PROJECT_ROOT, env=env)
    except FileNotFoundError as exc:
        print(f"Command tidak ditemukan: {exc}", file=sys.stderr)
        return 127
    return completed.returncode


def build(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    maybe_set_target(command, args.target, args.skip_set_target)
    command += ["build"]
    return run_idf(command)


def flash(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    if args.port:
        command += ["-p", args.port]
    maybe_set_target(command, args.target, args.skip_set_target)
    command += ["flash"]
    if args.monitor:
        command += ["monitor"]
    return run_idf(command)


def monitor(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    if args.port:
        command += ["-p", args.port]
    command += ["monitor"]
    return run_idf(command)


def erase(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    if args.port:
        command += ["-p", args.port]
    command += ["erase-flash"]
    return run_idf(command)


def menuconfig(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    maybe_set_target(command, args.target, args.skip_set_target)
    command += ["menuconfig"]
    return run_idf(command)


def clean(args: argparse.Namespace) -> int:
    command = target_args(args.target)
    command += ["fullclean" if args.full else "clean"]
    return run_idf(command)


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "-t",
        "--target",
        choices=SUPPORTED_TARGETS,
        default=os.environ.get("LED_CONTROLLER_TARGET", "esp32"),
        help="ESP-IDF chip target. Default: esp32 or LED_CONTROLLER_TARGET.",
    )
    parser.add_argument(
        "--skip-set-target",
        action="store_true",
        help="Do not run idf.py set-target before this command.",
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and flash LED Controller firmware on Windows, Linux, or macOS."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="Build firmware.")
    add_common_args(build_parser)
    build_parser.set_defaults(func=build)

    flash_parser = subparsers.add_parser("flash", help="Flash firmware.")
    add_common_args(flash_parser)
    flash_parser.add_argument("-p", "--port", help="Serial port, for example COM5 or /dev/ttyUSB0.")
    flash_parser.add_argument("--monitor", action="store_true", help="Open monitor after flashing.")
    flash_parser.set_defaults(func=flash)

    monitor_parser = subparsers.add_parser("monitor", help="Open ESP-IDF serial monitor.")
    monitor_parser.add_argument(
        "-t",
        "--target",
        choices=SUPPORTED_TARGETS,
        default=os.environ.get("LED_CONTROLLER_TARGET", "esp32"),
    )
    monitor_parser.add_argument("-p", "--port", help="Serial port.")
    monitor_parser.set_defaults(func=monitor)

    erase_parser = subparsers.add_parser("erase", help="Erase flash.")
    erase_parser.add_argument(
        "-t",
        "--target",
        choices=SUPPORTED_TARGETS,
        default=os.environ.get("LED_CONTROLLER_TARGET", "esp32"),
    )
    erase_parser.add_argument("-p", "--port", help="Serial port.")
    erase_parser.set_defaults(func=erase)

    menuconfig_parser = subparsers.add_parser("menuconfig", help="Open ESP-IDF menuconfig.")
    add_common_args(menuconfig_parser)
    menuconfig_parser.set_defaults(func=menuconfig)

    clean_parser = subparsers.add_parser("clean", help="Clean selected target build folder.")
    clean_parser.add_argument(
        "-t",
        "--target",
        choices=SUPPORTED_TARGETS,
        default=os.environ.get("LED_CONTROLLER_TARGET", "esp32"),
    )
    clean_parser.add_argument("--full", action="store_true", help="Run fullclean.")
    clean_parser.set_defaults(func=clean)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
