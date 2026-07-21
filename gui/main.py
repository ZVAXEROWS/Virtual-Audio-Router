"""
main.py — Virtual Audio Router
=================================
Application entry point.

Responsibilities:
  1. Configure Python logging
  2. Load the dark QSS theme
  3. Create the EngineBridge (loads C++ module or falls back to mock)
  4. Initialize the engine
  5. Show the main window
  6. Run the Qt event loop
  7. Clean shutdown
"""

from __future__ import annotations
import sys
import os
import logging

# Ensure gui/ package root is on the path
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt, QDir
from PySide6.QtGui import QFont, QFontDatabase


def _configure_logging() -> None:
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)-8s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )


def _load_stylesheet(app: QApplication) -> None:
    qss_path = os.path.join(_HERE, "resources", "styles", "dark_theme.qss")
    try:
        with open(qss_path, "r", encoding="utf-8") as f:
            app.setStyleSheet(f.read())
    except FileNotFoundError:
        logging.warning(f"dark_theme.qss not found at {qss_path}")


def _get_log_directory() -> str:
    """Return the directory where var_engine.log will be written."""
    app_data = os.environ.get("APPDATA", os.path.expanduser("~"))
    log_dir  = os.path.join(app_data, "VirtualAudioRouter", "logs")
    os.makedirs(log_dir, exist_ok=True)
    return log_dir


def main() -> int:
    _configure_logging()
    log = logging.getLogger("var.main")
    log.info("Virtual Audio Router starting up...")

    # High-DPI support
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)

    app = QApplication(sys.argv)
    app.setApplicationName("Virtual Audio Router")
    app.setOrganizationName("VAR")
    app.setApplicationVersion("0.1.0")

    # Prevent the app from quitting when the last window is closed
    # (we live in the system tray)
    app.setQuitOnLastWindowClosed(False)

    _load_stylesheet(app)

    # Late import — after path setup
    from core.engine_bridge import EngineBridge
    from core.settings import AppSettings
    from app.main_window import MainWindow

    # Create engine bridge (loads C++ module or mock)
    bridge = EngineBridge()

    # Initialize engine with log directory
    log_dir = _get_log_directory()
    if not bridge.initialize(log_dir):
        log.error("Failed to initialize audio engine. Exiting.")
        return 1

    log.info(f"Engine initialized. Mock={bridge.is_mock}")

    # Create settings
    settings = AppSettings()

    # Show main window
    window = MainWindow(bridge, settings)

    start_minimized = settings.load_start_minimized()
    if start_minimized:
        log.info("Starting minimized to tray.")
    else:
        window.show()

    exit_code = app.exec()
    log.info(f"Application exiting with code {exit_code}.")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
