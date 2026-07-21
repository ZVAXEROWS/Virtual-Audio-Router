"""
profile_manager.py — Virtual Audio Router
==========================================
Profile management dialog. Allows saving and loading named routing presets.
Phase 1: UI skeleton. Phase 7: Full save/load with JSON profiles.
"""

from __future__ import annotations
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QListWidget, QListWidgetItem,
    QPushButton, QLabel, QInputDialog, QMessageBox, QDialogButtonBox
)
from PySide6.QtCore import Qt, Signal


class ProfileManagerDialog(QDialog):
    profile_selected = Signal(str)  # emits profile name when user clicks Load

    def __init__(self, settings, parent=None):
        super().__init__(parent)
        self._settings = settings
        self.setWindowTitle("Profile Manager")
        self.setMinimumSize(360, 300)
        self.setModal(True)
        self._setup_ui()
        self._load_profiles()

    def _setup_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(12)
        layout.setContentsMargins(16, 16, 16, 16)

        layout.addWidget(QLabel("Saved Profiles:"))

        self._list = QListWidget()
        layout.addWidget(self._list)

        # Buttons
        btn_layout = QHBoxLayout()

        save_btn = QPushButton("Save Current")
        save_btn.clicked.connect(self._save_current)

        load_btn = QPushButton("Load Selected")
        load_btn.setProperty("class", "primary")
        load_btn.clicked.connect(self._load_selected)

        delete_btn = QPushButton("Delete")
        delete_btn.setProperty("class", "danger")
        delete_btn.clicked.connect(self._delete_selected)

        btn_layout.addWidget(save_btn)
        btn_layout.addWidget(load_btn)
        btn_layout.addStretch()
        btn_layout.addWidget(delete_btn)
        layout.addLayout(btn_layout)

        close_btn = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        close_btn.rejected.connect(self.reject)
        layout.addWidget(close_btn)

    def _load_profiles(self) -> None:
        """Phase 7: load from JSON. Phase 1: hardcoded default."""
        self._list.clear()
        self._list.addItem("Default")

    def _save_current(self) -> None:
        name, ok = QInputDialog.getText(self, "Save Profile", "Profile name:")
        if ok and name.strip():
            self._list.addItem(name.strip())
            self._settings.save_active_profile(name.strip())

    def _load_selected(self) -> None:
        item = self._list.currentItem()
        if item:
            self.profile_selected.emit(item.text())
            self.accept()

    def _delete_selected(self) -> None:
        item = self._list.currentItem()
        if item and item.text() != "Default":
            row = self._list.row(item)
            self._list.takeItem(row)
