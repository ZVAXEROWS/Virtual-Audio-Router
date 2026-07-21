"""
engine_bridge.py — Virtual Audio Router
=======================================
Thin wrapper around the var_engine C++ pybind11 module.

WHY THIS EXISTS:
  - Provides a clean, typed Python API surface on top of the raw pybind11 module.
  - Centralises all cross-bridge calls so they can be audited easily.

RULES:
  - This module must NEVER process audio data.
  - All calls are safe to make from the UI thread.
"""

from __future__ import annotations
import os
import sys
import logging
from typing import Any, Callable

logger = logging.getLogger("var.bridge")

# ---------------------------------------------------------------------------
# Import the C++ extension
# ---------------------------------------------------------------------------

try:
    # The .pyd is output to the gui/ directory by CMake
    _gui_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if _gui_dir not in sys.path:
        sys.path.insert(0, _gui_dir)

    import var_engine  # type: ignore
    logger.info("var_engine C++ module loaded successfully.")

except ImportError as e:
    logger.error(f"var_engine C++ module not found ({e}). Did you build the project?")
    raise


# ---------------------------------------------------------------------------
# EngineBridge — public API
# ---------------------------------------------------------------------------

class EngineBridge:
    """
    Wraps the real C++ AudioEngine.
    All UI code imports and uses EngineBridge exclusively.
    """

    def __init__(self):
        self._engine = var_engine.AudioEngine()

    @property
    def is_mock(self) -> bool:
        return False

    # -------------------------------------------------------------------------
    # Lifecycle
    # -------------------------------------------------------------------------

    def initialize(self, log_directory: str = ".") -> bool:
        """Initialize the engine. Returns True on success."""
        try:
            self._engine.initialize(log_directory)
            return True
        except Exception as e:
            logger.error(f"Engine initialize failed: {e}")
            return False

    def shutdown(self) -> None:
        try:
            self._engine.shutdown()
        except Exception as e:
            logger.error(f"Engine shutdown failed: {e}")

    # -------------------------------------------------------------------------
    # Callbacks
    # -------------------------------------------------------------------------

    def set_device_change_callback(self, callback: Callable[[], None]) -> None:
        try:
            self._engine.set_device_change_callback(callback)
        except Exception as e:
            logger.error(f"set_device_change_callback failed: {e}")

    # -------------------------------------------------------------------------
    # Devices
    # -------------------------------------------------------------------------

    def get_devices(self) -> list[dict[str, Any]]:
        try:
            return self._engine.get_devices()
        except Exception as e:
            logger.error(f"get_devices failed: {e}")
            return []

    def get_default_device(self) -> dict[str, Any] | None:
        try:
            return self._engine.get_default_device()
        except Exception:
            return None

    # -------------------------------------------------------------------------
    # Routing
    # -------------------------------------------------------------------------

    def start_routing(self, config: dict) -> bool:
        try:
            self._engine.start_routing(config)
            return True
        except Exception as e:
            logger.error(f"start_routing failed: {e}")
            return False

    def stop_routing(self) -> bool:
        try:
            self._engine.stop_routing()
            return True
        except Exception as e:
            logger.error(f"stop_routing failed: {e}")
            return False

    # -------------------------------------------------------------------------
    # Status
    # -------------------------------------------------------------------------

    def get_status(self) -> int:
        try:
            return int(self._engine.get_status())
        except Exception:
            return 0

    def get_status_name(self) -> str:
        names = {
            0: "Uninitialized", 1: "Initializing", 2: "Ready",
            3: "Routing", 4: "Stopping", 5: "Error", 6: "Shutting Down",
        }
        return names.get(self.get_status(), "Unknown")

    # -------------------------------------------------------------------------
    # Logs
    # -------------------------------------------------------------------------

    def get_recent_logs(self, max_entries: int = 100) -> list[dict]:
        try:
            return self._engine.get_recent_logs(max_entries)
        except Exception as e:
            logger.error(f"get_recent_logs failed: {e}")
            return []

    def get_current_config(self) -> dict:
        try:
            return self._engine.get_current_config()
        except Exception:
            return {}
