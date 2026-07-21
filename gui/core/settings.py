"""
settings.py — Virtual Audio Router
====================================
QSettings-backed persistent configuration for the GUI.

Stores: window geometry, selected output devices, active profile, theme.
Never stores audio data or engine internals.
"""

from __future__ import annotations
from PySide6.QtCore import QSettings, QByteArray


APP_NAME = "VirtualAudioRouter"
ORG_NAME = "VAR"


class AppSettings:
    """Wraps QSettings with typed accessors for all UI preferences."""

    def __init__(self):
        self._s = QSettings(ORG_NAME, APP_NAME)

    # -------------------------------------------------------------------------
    # Window geometry
    # -------------------------------------------------------------------------

    def save_geometry(self, geometry: QByteArray) -> None:
        self._s.setValue("window/geometry", geometry)

    def load_geometry(self) -> QByteArray | None:
        v = self._s.value("window/geometry")
        return v if isinstance(v, QByteArray) else None

    # -------------------------------------------------------------------------
    # Devices
    # -------------------------------------------------------------------------

    def save_selected_devices(self, device_ids: list[str]) -> None:
        self._s.setValue("routing/selected_device_ids", device_ids)

    def load_selected_devices(self) -> list[str]:
        v = self._s.value("routing/selected_device_ids", [])
        return v if isinstance(v, list) else []

    def save_input_device(self, device_id: str) -> None:
        self._s.setValue("routing/input_device_id", device_id)

    def load_input_device(self) -> str:
        return self._s.value("routing/input_device_id", "") or ""

    # -------------------------------------------------------------------------
    # Routing options
    # -------------------------------------------------------------------------

    def save_buffer_size_ms(self, ms: int) -> None:
        self._s.setValue("routing/buffer_size_ms", ms)

    def load_buffer_size_ms(self) -> int:
        return int(self._s.value("routing/buffer_size_ms", 20))

    def save_enable_resampling(self, enabled: bool) -> None:
        self._s.setValue("routing/enable_resampling", enabled)

    def load_enable_resampling(self) -> bool:
        v = self._s.value("routing/enable_resampling", True)
        return v if isinstance(v, bool) else str(v).lower() != "false"

    # -------------------------------------------------------------------------
    # Profiles
    # -------------------------------------------------------------------------

    def save_active_profile(self, name: str) -> None:
        self._s.setValue("profiles/active", name)

    def load_active_profile(self) -> str:
        return self._s.value("profiles/active", "Default") or "Default"

    # -------------------------------------------------------------------------
    # UI
    # -------------------------------------------------------------------------

    def save_start_minimized(self, minimized: bool) -> None:
        self._s.setValue("ui/start_minimized", minimized)

    def load_start_minimized(self) -> bool:
        v = self._s.value("ui/start_minimized", False)
        return v if isinstance(v, bool) else str(v).lower() == "true"

    def save_log_level(self, level: int) -> None:
        self._s.setValue("ui/log_level", level)

    def load_log_level(self) -> int:
        return int(self._s.value("ui/log_level", 1))  # Info = 1

    def sync(self) -> None:
        self._s.sync()
