"""
settings_dialog.py — Virtual Audio Router
==========================================
Settings dialog for buffer size, log level, and startup options.
"""

from __future__ import annotations
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QGroupBox, QLabel,
    QSpinBox, QComboBox, QCheckBox, QDialogButtonBox, QFormLayout
)
from PySide6.QtCore import Qt


class SettingsDialog(QDialog):
    def __init__(self, settings, parent=None):
        super().__init__(parent)
        self._settings = settings
        self.setWindowTitle("Settings — Virtual Audio Router")
        self.setMinimumWidth(400)
        self.setModal(True)
        self._setup_ui()
        self._load_values()

    def _setup_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(16)
        layout.setContentsMargins(20, 20, 20, 20)

        # --- Audio settings ---
        audio_group = QGroupBox("Audio")
        audio_form = QFormLayout(audio_group)
        audio_form.setSpacing(10)

        self._buffer_spin = QSpinBox()
        self._buffer_spin.setRange(5, 200)
        self._buffer_spin.setSuffix(" ms")
        self._buffer_spin.setToolTip(
            "Buffer size in milliseconds. Smaller = lower latency but more CPU.")
        audio_form.addRow("Buffer Size:", self._buffer_spin)

        self._resampling_check = QCheckBox("Enable automatic resampling")
        self._resampling_check.setToolTip(
            "Convert audio to match each device's native sample rate.")
        audio_form.addRow("", self._resampling_check)

        layout.addWidget(audio_group)

        # --- Logging ---
        log_group = QGroupBox("Logging")
        log_form = QFormLayout(log_group)

        self._log_level_combo = QComboBox()
        self._log_level_combo.addItems(["Debug", "Info", "Warning", "Error"])
        log_form.addRow("Log Level:", self._log_level_combo)

        layout.addWidget(log_group)

        # --- Startup ---
        startup_group = QGroupBox("Startup")
        startup_form = QFormLayout(startup_group)

        self._start_minimized_check = QCheckBox("Start minimized to tray")
        startup_form.addRow("", self._start_minimized_check)

        layout.addWidget(startup_group)

        # --- Buttons ---
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._save_and_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _load_values(self) -> None:
        self._buffer_spin.setValue(self._settings.load_buffer_size_ms())
        self._resampling_check.setChecked(self._settings.load_enable_resampling())
        self._log_level_combo.setCurrentIndex(self._settings.load_log_level())
        self._start_minimized_check.setChecked(self._settings.load_start_minimized())

    def _save_and_accept(self) -> None:
        self._settings.save_buffer_size_ms(self._buffer_spin.value())
        self._settings.save_enable_resampling(self._resampling_check.isChecked())
        self._settings.save_log_level(self._log_level_combo.currentIndex())
        self._settings.save_start_minimized(self._start_minimized_check.isChecked())
        self._settings.sync()
        self.accept()
