"""Application entry point and argument parsing.

Usage:
    uv run loader-host                 # pick a port in the GUI
    uv run loader-host --sim           # built-in simulator, no hardware
    uv run loader-host --theme dark    # force a theme (default light, persisted)
    uv run loader-host -p COM7         # connect to a real port
    uv run loader-host -p COM7 -b 115200
"""

from __future__ import annotations

import argparse
import sys

from . import config


def build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="loader-host",
        description="Electronic-load host application (GUI) for the STM32H750 firmware.",
    )
    parser.add_argument(
        "-p",
        "--port",
        default=None,
        help="serial port to open on start (e.g. COM7). Omit to choose in the GUI.",
    )
    parser.add_argument(
        "-b",
        "--baudrate",
        type=int,
        default=config.DEFAULT_BAUDRATE,
        help=f"baud rate (default {config.DEFAULT_BAUDRATE})",
    )
    parser.add_argument(
        "--sim",
        action="store_true",
        help="start with the built-in device simulator instead of a serial port",
    )
    parser.add_argument(
        "--theme",
        choices=("light", "dark"),
        default=None,
        help="UI theme (default light; the in-app toggle persists the choice)",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="print detected serial ports and exit",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_argparser().parse_args(argv)

    if args.list_ports:
        from .client import list_serial_ports

        ports = list_serial_ports()
        if not ports:
            print("no serial ports detected")
        for device, label in ports:
            print(f"{device}\t{label}")
        return 0

    # Imported after arg parsing so --list-ports works without a display.
    from PySide6.QtCore import QSettings
    from PySide6.QtWidgets import QApplication

    from . import theme
    from .main_window import MainWindow

    app = QApplication(sys.argv[:1])
    app.setApplicationName("loader-host")
    app.setOrganizationName("BaseFramework")

    # CLI wins; otherwise the last toggle from QSettings; default light.
    theme_name = args.theme or QSettings("BaseFramework", "loader_host").value(
        "theme", "light"
    )
    theme.apply(app, theme_name)

    window = MainWindow(start_sim=args.sim, port=args.port, baudrate=args.baudrate)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
