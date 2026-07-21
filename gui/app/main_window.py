"""
main_window.py — Virtual Audio Router
========================================
Main application window. Orchestrates all panels and connects them to
the engine bridge.

Layout:
  ┌─────────────────────────────────────────────────┐
  │  Menu Bar                                       │
  ├────────────────────────┬────────────────────────┤
  │                        │  Status Panel          │
  │  Device Panel          │  (engine status,       │
  │  (output checkboxes)   │   routing controls)    │
  │                        ├────────────────────────┤
  │                        │  Log Panel             │
  │                        │  (ring buffer viewer)  │
  ├────────────────────────┴────────────────────────┤
  │  Status Bar                                     │
  └─────────────────────────────────────────────────┘
"""

from __future__ import annotations
import os
import sys

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QSplitter,
    QFrame, QLabel, QPushButton, QTextEdit, QStatusBar,
    QMenuBar, QMenu, QToolBar, QSizePolicy
)
from PySide6.QtCore import Qt, QTimer, Signal, QSize
from PySide6.QtGui import QAction, QColor, QTextCharFormat, QFont

from core.engine_bridge import EngineBridge
from core.settings import AppSettings
from core.logger import EngineLogPoller, QtLogHandler
from app.device_panel import DevicePanel
from app.tray_icon import TrayIcon
from app.settings_dialog import SettingsDialog
from app.profile_manager import ProfileManagerDialog


_STATUS_COLORS = {
    0: "#8b90a4",   # Uninitialized
    1: "#f5a623",   # Initializing
    2: "#3ddc84",   # Ready
    3: "#4f8ef7",   # Routing
    4: "#f5a623",   # Stopping
    5: "#e05252",   # Error
    6: "#f5a623",   # Shutting Down
}

_LOG_LEVEL_COLORS = {
    0: "#8b90a4",   # Debug — muted
    1: "#e8eaf0",   # Info — normal
    2: "#f5a623",   # Warning — amber
    3: "#e05252",   # Error — red
    4: "#ff6b6b",   # Fatal — bright red
}

_LOG_LEVEL_NAMES = ["DBG", "INF", "WRN", "ERR", "FTL"]


class MainWindow(QMainWindow):
    def __init__(self, bridge: EngineBridge, settings: AppSettings):
        super().__init__()
        self._bridge    = bridge
        self._settings  = settings
        self._selected_device_ids: list[str] = []
        self._is_routing = False

        self.setWindowTitle("Virtual Audio Router")
        self.setMinimumSize(1000, 640)

        self._setup_ui()
        self._setup_menu()
        self._setup_tray()
        self._setup_timers()
        self._restore_state()
        self._refresh_devices()

    # =========================================================================
    # UI construction
    # =========================================================================

    def _setup_ui(self) -> None:
        # ---- Central widget -------------------------------------------------
        central = QWidget()
        self.setCentralWidget(central)
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        # ---- Toolbar --------------------------------------------------------
        toolbar = self._make_toolbar()
        root_layout.addWidget(toolbar)

        # ---- Main splitter --------------------------------------------------
        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setChildrenCollapsible(False)
        root_layout.addWidget(splitter)

        # Left: device panel
        self._device_panel = DevicePanel()
        self._device_panel.setMinimumWidth(380)
        self._device_panel.selected_devices_changed.connect(self._on_devices_changed)
        self._device_panel.refresh_requested.connect(self._refresh_devices)
        splitter.addWidget(self._device_panel)

        # Right: control + log
        right_pane = QWidget()
        right_layout = QVBoxLayout(right_pane)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(0)

        right_layout.addWidget(self._make_control_panel())
        right_layout.addWidget(self._make_log_panel(), stretch=1)
        splitter.addWidget(right_pane)

        splitter.setSizes([420, 580])

        # ---- Status bar -----------------------------------------------------
        self._status_bar = QStatusBar()
        self.setStatusBar(self._status_bar)

        self._status_engine_label = QLabel("Engine: Initializing...")
        self._status_engine_label.setStyleSheet("color: #8b90a4; padding: 0 8px;")

        self._status_mode_label = QLabel()
        self._status_mode_label.setStyleSheet("color: #f5a623; padding: 0 8px;")

        self._status_bar.addWidget(self._status_engine_label)
        self._status_bar.addPermanentWidget(self._status_mode_label)

    def _make_toolbar(self) -> QFrame:
        toolbar = QFrame()
        toolbar.setObjectName("mainToolbar")
        toolbar.setStyleSheet("""
            QFrame#mainToolbar {
                background: #161921;
                border-bottom: 1px solid #2e3347;
                min-height: 52px;
                max-height: 52px;
            }
        """)
        layout = QHBoxLayout(toolbar)
        layout.setContentsMargins(16, 8, 16, 8)
        layout.setSpacing(12)

        # App logo/title
        title_label = QLabel("⟳  Virtual Audio Router")
        title_label.setStyleSheet("""
            font-size: 16px;
            font-weight: 700;
            color: #e8eaf0;
            letter-spacing: 0.5px;
        """)
        layout.addWidget(title_label)

        # Mock badge
        if self._bridge.is_mock:
            mock_badge = QLabel("DEV MODE")
            mock_badge.setStyleSheet("""
                background: #2a2000;
                color: #f5a623;
                border: 1px solid #4a3a00;
                border-radius: 4px;
                padding: 2px 8px;
                font-size: 10px;
                font-weight: 700;
                letter-spacing: 1px;
            """)
            layout.addWidget(mock_badge)

        layout.addStretch()

        # Status indicator
        self._status_dot = QLabel("●")
        self._status_dot.setStyleSheet("font-size: 16px; color: #3ddc84;")

        self._status_text = QLabel("Ready")
        self._status_text.setStyleSheet("font-size: 13px; color: #8b90a4; font-weight: 500;")

        layout.addWidget(self._status_dot)
        layout.addWidget(self._status_text)

        # Separator
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setStyleSheet("color: #2e3347;")
        layout.addWidget(sep)

        # Start/Stop routing button
        self._route_btn = QPushButton("▶  Start Routing")
        self._route_btn.setProperty("class", "primary")
        self._route_btn.setFixedHeight(34)
        self._route_btn.setMinimumWidth(140)
        self._route_btn.setEnabled(False)
        self._route_btn.clicked.connect(self._toggle_routing)
        self._route_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #5a99ff, stop:1 #3a7ae0);
                border: 1px solid #3a7ae0;
                border-radius: 6px;
                color: #ffffff;
                font-weight: 600;
                font-size: 13px;
                padding: 6px 18px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #6ba3ff, stop:1 #4f8ef7);
            }
            QPushButton:disabled {
                background: #252a38;
                border-color: #2e3347;
                color: #4a4f63;
            }
        """)
        layout.addWidget(self._route_btn)

        return toolbar

    def _make_control_panel(self) -> QFrame:
        panel = QFrame()
        panel.setObjectName("controlPanel")
        panel.setStyleSheet("""
            QFrame#controlPanel {
                background: #1e2230;
                border-bottom: 1px solid #2e3347;
            }
        """)
        panel.setFixedHeight(120)
        layout = QHBoxLayout(panel)
        layout.setContentsMargins(20, 16, 20, 16)
        layout.setSpacing(24)

        # Stat: selected outputs
        layout.addWidget(self._make_stat("OUTPUTS SELECTED", "0"))
        self._stat_outputs = layout.itemAt(layout.count()-1).widget().findChild(QLabel, "stat_value")

        layout.addWidget(self._make_vline())

        # Stat: engine status
        layout.addWidget(self._make_stat("ENGINE STATUS", "Ready"))
        self._stat_status = layout.itemAt(layout.count()-1).widget().findChild(QLabel, "stat_value")

        layout.addWidget(self._make_vline())

        # Stat: buffer size
        layout.addWidget(self._make_stat("BUFFER", "20ms"))
        self._stat_buffer = layout.itemAt(layout.count()-1).widget().findChild(QLabel, "stat_value")

        layout.addStretch()

        return panel

    def _make_stat(self, label: str, value: str) -> QWidget:
        container = QWidget()
        vl = QVBoxLayout(container)
        vl.setContentsMargins(0, 0, 0, 0)
        vl.setSpacing(4)

        lbl = QLabel(label)
        lbl.setStyleSheet("""
            font-size: 10px;
            font-weight: 700;
            color: #8b90a4;
            letter-spacing: 1.2px;
        """)

        val = QLabel(value)
        val.setObjectName("stat_value")
        val.setStyleSheet("font-size: 22px; font-weight: 700; color: #e8eaf0;")

        vl.addWidget(lbl)
        vl.addWidget(val)
        return container

    def _make_vline(self) -> QFrame:
        line = QFrame()
        line.setFrameShape(QFrame.Shape.VLine)
        line.setStyleSheet("color: #2e3347;")
        return line

    def _make_log_panel(self) -> QFrame:
        panel = QFrame()
        panel.setObjectName("logPanel")
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Log header
        log_header = QFrame()
        log_header.setStyleSheet("""
            QFrame { background: #161921; border-bottom: 1px solid #2e3347; }
        """)
        lh_layout = QHBoxLayout(log_header)
        lh_layout.setContentsMargins(16, 8, 12, 8)

        log_title = QLabel("Engine Log")
        log_title.setStyleSheet("font-size: 12px; font-weight: 600; color: #8b90a4; letter-spacing: 0.5px;")

        clear_btn = QPushButton("Clear")
        clear_btn.setFixedHeight(22)
        clear_btn.setStyleSheet("""
            QPushButton {
                background: transparent; border: 1px solid #2e3347;
                border-radius: 3px; padding: 2px 8px;
                color: #8b90a4; font-size: 11px;
            }
            QPushButton:hover { border-color: #4f8ef7; color: #4f8ef7; }
        """)
        clear_btn.clicked.connect(self._clear_log)

        lh_layout.addWidget(log_title)
        lh_layout.addStretch()
        lh_layout.addWidget(clear_btn)
        layout.addWidget(log_header)

        # Log text area
        self._log_view = QTextEdit()
        self._log_view.setReadOnly(True)
        self._log_view.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        layout.addWidget(self._log_view)

        return panel

    # =========================================================================
    # Menu
    # =========================================================================

    def _setup_menu(self) -> None:
        mb = self.menuBar()

        # File
        file_menu = mb.addMenu("&File")
        file_menu.addAction("&Settings...", self._open_settings, "Ctrl+,")
        file_menu.addAction("&Profiles...", self._open_profiles, "Ctrl+P")
        file_menu.addSeparator()
        file_menu.addAction("&Quit", self.close, "Ctrl+Q")

        # Devices
        dev_menu = mb.addMenu("&Devices")
        dev_menu.addAction("&Refresh Devices", self._refresh_devices, "F5")

        # Routing
        route_menu = mb.addMenu("&Routing")
        self._start_action = route_menu.addAction("&Start Routing", self._toggle_routing)
        route_menu.addAction("S&top Routing", self._stop_routing)

        # Help
        help_menu = mb.addMenu("&Help")
        help_menu.addAction("About Virtual Audio Router", self._show_about)

    # =========================================================================
    # Tray
    # =========================================================================

    def _setup_tray(self) -> None:
        self._tray = TrayIcon(self)
        self._tray.show_requested.connect(self._restore_window)
        self._tray.quit_requested.connect(self.close)
        self._tray.show()

    # =========================================================================
    # Timers
    # =========================================================================

    def _setup_timers(self) -> None:
        # Poll engine status every 500ms
        self._status_timer = QTimer(self)
        self._status_timer.setInterval(500)
        self._status_timer.timeout.connect(self._poll_status)
        self._status_timer.start()

        # Poll log ring buffer every 500ms
        self._log_poller = EngineLogPoller(self._bridge, self)
        self._log_poller.new_log_entry.connect(self._append_log)
        self._log_timer = QTimer(self)
        self._log_timer.setInterval(500)
        self._log_timer.timeout.connect(self._log_poller.poll)
        self._log_timer.start()

    # =========================================================================
    # State management
    # =========================================================================

    def _restore_state(self) -> None:
        geo = self._settings.load_geometry()
        if geo:
            self.restoreGeometry(geo)
        saved_ids = self._settings.load_selected_devices()
        if saved_ids:
            self._selected_device_ids = saved_ids

    def _refresh_devices(self) -> None:
        devices = self._bridge.get_devices()
        self._device_panel.load_devices(devices)
        if self._selected_device_ids:
            self._device_panel.restore_selection(self._selected_device_ids)
        self._append_log_line(1, "devices", f"Loaded {len(devices)} device(s).")

    # =========================================================================
    # Routing control
    # =========================================================================

    def _on_devices_changed(self, device_ids: list[str]) -> None:
        self._selected_device_ids = device_ids
        self._settings.save_selected_devices(device_ids)
        count = len(device_ids)
        if self._stat_outputs:
            self._stat_outputs.setText(str(count))
        self._route_btn.setEnabled(count > 0 and not self._is_routing)
        self._tray.set_routing_action_enabled(count > 0)

    def _toggle_routing(self) -> None:
        if self._is_routing:
            self._stop_routing()
        else:
            self._start_routing()

    def _start_routing(self) -> None:
        config = {
            "input_device_id": "",
            "output_device_ids": self._selected_device_ids,
            "buffer_size_ms": self._settings.load_buffer_size_ms(),
            "enable_resampling": self._settings.load_enable_resampling(),
            "enable_drift_correction": True,
        }
        success = self._bridge.start_routing(config)
        if success:
            self._is_routing = True
            self._route_btn.setText("■  Stop Routing")
            self._route_btn.setStyleSheet("""
                QPushButton {
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 #3ddc84, stop:1 #2ab870);
                    border: 1px solid #2ab870;
                    border-radius: 6px;
                    color: #ffffff;
                    font-weight: 600;
                    font-size: 13px;
                    padding: 6px 18px;
                }
                QPushButton:hover {
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 #4feb96, stop:1 #3ddc84);
                }
            """)
            self._tray.set_routing(True)

    def _stop_routing(self) -> None:
        self._bridge.stop_routing()
        self._is_routing = False
        self._route_btn.setText("▶  Start Routing")
        # Restore original style
        self._route_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #5a99ff, stop:1 #3a7ae0);
                border: 1px solid #3a7ae0;
                border-radius: 6px;
                color: #ffffff;
                font-weight: 600;
                font-size: 13px;
                padding: 6px 18px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #6ba3ff, stop:1 #4f8ef7);
            }
            QPushButton:disabled {
                background: #252a38;
                border-color: #2e3347;
                color: #4a4f63;
            }
        """)
        self._tray.set_routing(False)

    # =========================================================================
    # Status polling
    # =========================================================================

    def _poll_status(self) -> None:
        status_int  = self._bridge.get_status()
        status_name = self._bridge.get_status_name()
        color       = _STATUS_COLORS.get(status_int, "#8b90a4")

        self._status_dot.setStyleSheet(f"font-size: 16px; color: {color};")
        self._status_text.setText(status_name)
        self._status_text.setStyleSheet(f"font-size: 13px; color: {color}; font-weight: 500;")
        self._status_engine_label.setText(f"Engine: {status_name}")

        if self._stat_status:
            self._stat_status.setText(status_name)
            self._stat_status.setStyleSheet(f"font-size: 22px; font-weight: 700; color: {color};")

        buf_ms = self._settings.load_buffer_size_ms()
        if self._stat_buffer:
            self._stat_buffer.setText(f"{buf_ms}ms")

        if self._bridge.is_mock:
            self._status_mode_label.setText("⚠  Mock Mode — build C++ engine for real audio")

    # =========================================================================
    # Logging
    # =========================================================================

    def _append_log(self, entry: dict) -> None:
        level  = entry.get("level", 1)
        source = entry.get("source", "")
        msg    = entry.get("message", "")
        ts     = entry.get("timestamp_ms", 0)
        self._append_log_line(level, source, msg, ts)

    def _append_log_line(self, level: int, source: str, msg: str,
                          ts: int = 0) -> None:
        color     = _LOG_LEVEL_COLORS.get(level, "#e8eaf0")
        level_tag = _LOG_LEVEL_NAMES[level] if level < len(_LOG_LEVEL_NAMES) else "???"

        html = (
            f'<span style="color:#4a4f63;">[{ts:>8}ms]</span> '
            f'<span style="color:{color}; font-weight:600;">[{level_tag}]</span> '
            f'<span style="color:#6b7090;">[{source:<20}]</span> '
            f'<span style="color:{color};">{msg}</span>'
        )
        self._log_view.append(html)

        # Auto-scroll to bottom
        sb = self._log_view.verticalScrollBar()
        sb.setValue(sb.maximum())

    def _clear_log(self) -> None:
        self._log_view.clear()

    # =========================================================================
    # Dialogs
    # =========================================================================

    def _open_settings(self) -> None:
        dlg = SettingsDialog(self._settings, self)
        dlg.exec()

    def _open_profiles(self) -> None:
        dlg = ProfileManagerDialog(self._settings, self)
        dlg.exec()

    def _show_about(self) -> None:
        from PySide6.QtWidgets import QMessageBox
        QMessageBox.about(self, "About Virtual Audio Router",
            "<b>Virtual Audio Router v0.1.0</b><br><br>"
            "Route one audio source to multiple output devices simultaneously.<br><br>"
            "Built with C++20 · WASAPI · PySide6 · pybind11<br><br>"
            "Phase 1 — Architecture skeleton.")

    # =========================================================================
    # Window events
    # =========================================================================

    def _restore_window(self) -> None:
        self.showNormal()
        self.raise_()
        self.activateWindow()

    def closeEvent(self, event) -> None:
        self._settings.save_geometry(self.saveGeometry())
        self._settings.sync()
        self._status_timer.stop()
        self._log_timer.stop()
        self._bridge.shutdown()
        self._tray.hide()
        event.accept()
