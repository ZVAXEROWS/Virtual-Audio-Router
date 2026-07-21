"""
engine_bridge.py — Virtual Audio Router
=======================================
Thin wrapper around the var_engine C++ pybind11 module.

WHY THIS EXISTS:
  - Catches ImportError if the C++ module hasn't been built yet and provides
    a full mock implementation so UI developers can iterate without a C++ build.
  - Provides a clean, typed Python API surface on top of the raw pybind11 module.
  - Centralises all cross-bridge calls so they can be audited easily.

RULES:
  - This module must NEVER process audio data.
  - All calls are safe to make from the UI thread.
  - Methods that return device lists must return plain Python dicts.
"""

from __future__ import annotations
import os
import sys
import logging
from typing import Any

logger = logging.getLogger("var.bridge")

# ---------------------------------------------------------------------------
# Attempt to import the C++ extension
# ---------------------------------------------------------------------------

_ENGINE_AVAILABLE = False
_var_engine = None

try:
    # The .pyd is output to the gui/ directory by CMake
    _gui_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if _gui_dir not in sys.path:
        sys.path.insert(0, _gui_dir)

    import var_engine as _var_engine  # type: ignore
    _ENGINE_AVAILABLE = True
    logger.info("var_engine C++ module loaded successfully.")

except ImportError as e:
    logger.warning(
        f"var_engine C++ module not found ({e}). "
        "Running in mock/dev mode — no audio processing."
    )


# ---------------------------------------------------------------------------
# Mock implementation (used when C++ engine is not built)
# ---------------------------------------------------------------------------

class _MockEngineStatus:
    Uninitialized = 0
    Initializing  = 1
    Ready         = 2
    Routing       = 3
    Stopping      = 4
    Error         = 5
    ShuttingDown  = 6


MOCK_DEVICES = [
    {
        "id": "mock-device-0",
        "name": "Mock Speakers (Realtek HD Audio)",
        "description": "High Definition Audio Device",
        "state": 1,  # Active
        "is_default": True,
        "sample_rate": 48000,
        "channels": 2,
        "bits_per_sample": 32,
        "latency_ms": 10.0,
    },
    {
        "id": "mock-device-1",
        "name": "Mock Bluetooth Headphones",
        "description": "Bluetooth A2DP Audio",
        "state": 1,
        "is_default": False,
        "sample_rate": 44100,
        "channels": 2,
        "bits_per_sample": 16,
        "latency_ms": 80.0,
    },
    {
        "id": "mock-device-2",
        "name": "Mock USB DAC (Focusrite)",
        "description": "USB Audio Device",
        "state": 1,
        "is_default": False,
        "sample_rate": 48000,
        "channels": 2,
        "bits_per_sample": 32,
        "latency_ms": 5.0,
    },
    {
        "id": "mock-device-3",
        "name": "Mock HDMI Output (NVIDIA)",
        "description": "HDMI Audio",
        "state": 1,
        "is_default": False,
        "sample_rate": 48000,
        "channels": 6,
        "bits_per_sample": 32,
        "latency_ms": 15.0,
    },
]


class _MockAudioEngine:
    """Full mock implementation that mirrors the var_engine.AudioEngine API."""

    def __init__(self):
        self._status = _MockEngineStatus.Uninitialized
        self._config = {}
        self._logs: list[dict] = []
        self._ts = 0

    def _log(self, level: int, msg: str) -> None:
        import time
        self._ts += 50
        self._logs.append({
            "level": level,
            "message": msg,
            "source": "mock_engine.py",
            "timestamp_ms": self._ts,
        })

    def initialize(self, log_directory: str = ".") -> None:
        self._status = _MockEngineStatus.Ready
        self._log(1, f"[MOCK] Engine initialized. Log dir: {log_directory}")

    def shutdown(self) -> None:
        self._status = _MockEngineStatus.Uninitialized
        self._log(1, "[MOCK] Engine shutdown.")

    def get_devices(self) -> list[dict]:
        self._log(0, "[MOCK] get_devices() called — returning mock data.")
        return list(MOCK_DEVICES)

    def get_default_device(self) -> dict:
        return MOCK_DEVICES[0]

    def start_routing(self, config: dict) -> None:
        self._config = config
        self._status = _MockEngineStatus.Routing
        ids = config.get("output_device_ids", [])
        self._log(1, f"[MOCK] Routing started to {len(ids)} device(s).")

    def stop_routing(self) -> None:
        self._status = _MockEngineStatus.Ready
        self._log(1, "[MOCK] Routing stopped.")

    def get_status(self) -> int:
        return self._status

    def get_recent_logs(self, max_entries: int = 100) -> list[dict]:
        return self._logs[-max_entries:]

    def get_current_config(self) -> dict:
        return dict(self._config)


# ---------------------------------------------------------------------------
# EngineBridge — public API
# ---------------------------------------------------------------------------

class EngineBridge:
    """
    Wraps either the real C++ AudioEngine or the mock engine.
    All UI code imports and uses EngineBridge exclusively.
    """

    def __init__(self):
        if _ENGINE_AVAILABLE and _var_engine is not None:
            self._engine = _var_engine.AudioEngine()
            self._mock = False
        else:
            self._engine = _MockAudioEngine()
            self._mock = True

    @property
    def is_mock(self) -> bool:
        return self._mock

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
