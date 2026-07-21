"""
device_panel.py — Virtual Audio Router
========================================
Device list widget. Displays all detected audio endpoints in a tree view
with checkboxes for output selection.

In Phase 1: populated from engine_bridge (mock data if C++ not built).
In Phase 2: populated from real WASAPI enumeration.
"""

from __future__ import annotations
from typing import Any

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLabel, QFrame, QSizePolicy
)
from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QColor, QFont, QIcon


# Column indices
COL_NAME        = 0
COL_STATE       = 1
COL_DEFAULT     = 2
COL_SAMPLE_RATE = 3
COL_CHANNELS    = 4
COL_LATENCY     = 5


_STATE_LABELS = {
    1: ("Active",      "#3ddc84"),
    2: ("Disabled",    "#f5a623"),
    4: ("Not Present", "#e05252"),
    8: ("Unplugged",   "#f5a623"),
}


class DevicePanel(QWidget):
    """
    Left panel: displays all output devices with checkboxes.
    Emits selected_devices_changed(list[str]) when the user checks/unchecks a device.
    """

    selected_devices_changed = Signal(list)   # list of selected device IDs
    refresh_requested        = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self._devices: list[dict] = []
        self._selected_ids: set[str] = set()
        self._setup_ui()

    # -------------------------------------------------------------------------
    # UI construction
    # -------------------------------------------------------------------------

    def _setup_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # --- Header ---
        header = QFrame()
        header.setObjectName("panelHeader")
        header.setStyleSheet("""
            QFrame#panelHeader {
                background: #161921;
                border-bottom: 1px solid #2e3347;
                padding: 0;
            }
        """)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(16, 12, 12, 12)

        title = QLabel("Output Devices")
        title.setStyleSheet("font-size: 14px; font-weight: 700; color: #e8eaf0;")

        self._device_count_label = QLabel("0 devices")
        self._device_count_label.setStyleSheet("font-size: 11px; color: #8b90a4;")

        self._refresh_btn = QPushButton("⟳  Refresh")
        self._refresh_btn.setFixedHeight(28)
        self._refresh_btn.setStyleSheet("""
            QPushButton {
                background: transparent;
                border: 1px solid #2e3347;
                border-radius: 5px;
                padding: 4px 10px;
                color: #8b90a4;
                font-size: 12px;
            }
            QPushButton:hover {
                border-color: #4f8ef7;
                color: #4f8ef7;
            }
        """)
        self._refresh_btn.clicked.connect(self.refresh_requested)

        header_layout.addWidget(title)
        header_layout.addWidget(self._device_count_label)
        header_layout.addStretch()
        header_layout.addWidget(self._refresh_btn)
        layout.addWidget(header)

        # --- Tree ---
        self._tree = QTreeWidget()
        self._tree.setColumnCount(6)
        self._tree.setHeaderLabels([
            "Device Name", "State", "Default", "Sample Rate", "Channels", "Latency"
        ])
        self._tree.setAlternatingRowColors(True)
        self._tree.setRootIsDecorated(False)
        self._tree.setSortingEnabled(True)
        self._tree.setSelectionMode(QTreeWidget.SelectionMode.ExtendedSelection)

        # Column widths
        self._tree.setColumnWidth(COL_NAME,        260)
        self._tree.setColumnWidth(COL_STATE,        80)
        self._tree.setColumnWidth(COL_DEFAULT,      60)
        self._tree.setColumnWidth(COL_SAMPLE_RATE,  90)
        self._tree.setColumnWidth(COL_CHANNELS,     70)
        self._tree.setColumnWidth(COL_LATENCY,      80)

        self._tree.itemChanged.connect(self._on_item_changed)
        layout.addWidget(self._tree)

        # --- Footer: selection summary ---
        footer = QFrame()
        footer.setStyleSheet("""
            QFrame {
                background: #161921;
                border-top: 1px solid #2e3347;
            }
        """)
        footer_layout = QHBoxLayout(footer)
        footer_layout.setContentsMargins(16, 8, 16, 8)

        self._selection_label = QLabel("No outputs selected")
        self._selection_label.setStyleSheet("color: #8b90a4; font-size: 12px;")

        select_all_btn = QPushButton("Select All")
        select_all_btn.setFixedHeight(24)
        select_all_btn.setStyleSheet("""
            QPushButton {
                background: transparent; border: 1px solid #2e3347;
                border-radius: 4px; padding: 2px 8px;
                color: #8b90a4; font-size: 11px;
            }
            QPushButton:hover { border-color: #4f8ef7; color: #4f8ef7; }
        """)
        select_all_btn.clicked.connect(self._select_all)

        clear_btn = QPushButton("Clear")
        clear_btn.setFixedHeight(24)
        clear_btn.setStyleSheet(select_all_btn.styleSheet())
        clear_btn.clicked.connect(self._clear_selection)

        footer_layout.addWidget(self._selection_label)
        footer_layout.addStretch()
        footer_layout.addWidget(select_all_btn)
        footer_layout.addWidget(clear_btn)
        layout.addWidget(footer)

    # -------------------------------------------------------------------------
    # Data loading
    # -------------------------------------------------------------------------

    def load_devices(self, devices: list[dict]) -> None:
        """Populate the tree from a list of DeviceInfo dicts."""
        self._devices = devices
        self._tree.blockSignals(True)
        self._tree.clear()

        for dev in devices:
            item = self._make_item(dev)
            self._tree.addTopLevelItem(item)

        self._tree.blockSignals(False)
        self._update_count()

    def _make_item(self, dev: dict) -> QTreeWidgetItem:
        item = QTreeWidgetItem()
        item.setData(COL_NAME, Qt.ItemDataRole.UserRole, dev["id"])

        # Name (with checkbox)
        item.setText(COL_NAME, dev.get("name", "Unknown"))
        item.setCheckState(COL_NAME,
            Qt.CheckState.Checked if dev["id"] in self._selected_ids
            else Qt.CheckState.Unchecked)

        # State badge
        state_val = dev.get("state", 0)
        state_label, state_color = _STATE_LABELS.get(state_val, ("Unknown", "#8b90a4"))
        item.setText(COL_STATE, state_label)
        item.setForeground(COL_STATE, QColor(state_color))

        # Default
        item.setText(COL_DEFAULT, "★" if dev.get("is_default") else "")
        if dev.get("is_default"):
            item.setForeground(COL_DEFAULT, QColor("#f5a623"))

        # Sample rate
        sr = dev.get("sample_rate", 0)
        item.setText(COL_SAMPLE_RATE, f"{sr // 1000}kHz" if sr else "—")

        # Channels
        ch = dev.get("channels", 0)
        ch_labels = {1: "Mono", 2: "Stereo", 6: "5.1", 8: "7.1"}
        item.setText(COL_CHANNELS, ch_labels.get(ch, str(ch)))

        # Latency
        lat = dev.get("latency_ms", 0.0)
        item.setText(COL_LATENCY, f"{lat:.1f}ms" if lat > 0 else "—")

        return item

    # -------------------------------------------------------------------------
    # Selection management
    # -------------------------------------------------------------------------

    def _on_item_changed(self, item: QTreeWidgetItem, column: int) -> None:
        if column != COL_NAME:
            return
        device_id = item.data(COL_NAME, Qt.ItemDataRole.UserRole)
        if item.checkState(COL_NAME) == Qt.CheckState.Checked:
            self._selected_ids.add(device_id)
        else:
            self._selected_ids.discard(device_id)

        self._update_count()
        self.selected_devices_changed.emit(list(self._selected_ids))

    def _select_all(self) -> None:
        self._tree.blockSignals(True)
        for i in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(i)
            item.setCheckState(COL_NAME, Qt.CheckState.Checked)
            dev_id = item.data(COL_NAME, Qt.ItemDataRole.UserRole)
            self._selected_ids.add(dev_id)
        self._tree.blockSignals(False)
        self._update_count()
        self.selected_devices_changed.emit(list(self._selected_ids))

    def _clear_selection(self) -> None:
        self._tree.blockSignals(True)
        for i in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(i)
            item.setCheckState(COL_NAME, Qt.CheckState.Unchecked)
        self._selected_ids.clear()
        self._tree.blockSignals(False)
        self._update_count()
        self.selected_devices_changed.emit([])

    def restore_selection(self, device_ids: list[str]) -> None:
        """Restore a saved selection (called on startup)."""
        self._selected_ids = set(device_ids)
        self._tree.blockSignals(True)
        for i in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(i)
            dev_id = item.data(COL_NAME, Qt.ItemDataRole.UserRole)
            state = Qt.CheckState.Checked if dev_id in self._selected_ids \
                    else Qt.CheckState.Unchecked
            item.setCheckState(COL_NAME, state)
        self._tree.blockSignals(False)
        self._update_count()

    def get_selected_ids(self) -> list[str]:
        return list(self._selected_ids)

    # -------------------------------------------------------------------------
    # Internal helpers
    # -------------------------------------------------------------------------

    def _update_count(self) -> None:
        total    = self._tree.topLevelItemCount()
        selected = len(self._selected_ids)
        self._device_count_label.setText(f"{total} device{'s' if total != 1 else ''}")
        if selected == 0:
            self._selection_label.setText("No outputs selected")
            self._selection_label.setStyleSheet("color: #8b90a4; font-size: 12px;")
        else:
            self._selection_label.setText(
                f"{selected} output{'s' if selected != 1 else ''} selected")
            self._selection_label.setStyleSheet("color: #3ddc84; font-size: 12px;")
