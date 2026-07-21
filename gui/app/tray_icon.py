"""
tray_icon.py — Virtual Audio Router
=======================================
System tray icon with context menu.
"""

from __future__ import annotations
from PySide6.QtWidgets import QSystemTrayIcon, QMenu
from PySide6.QtGui import QIcon, QPixmap, QPainter, QColor, QBrush
from PySide6.QtCore import Signal, QSize, Qt


def _make_tray_icon(color: str = "#4f8ef7") -> QIcon:
    """Generate a simple colored circle icon programmatically."""
    px = QPixmap(QSize(32, 32))
    px.fill(Qt.GlobalColor.transparent)
    painter = QPainter(px)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setBrush(QBrush(QColor(color)))
    painter.setPen(Qt.PenStyle.NoPen)
    painter.drawEllipse(2, 2, 28, 28)
    painter.end()
    return QIcon(px)


class TrayIcon(QSystemTrayIcon):
    """
    System tray icon for Virtual Audio Router.
    Signals:
        show_requested()   — user clicked "Show"
        quit_requested()   — user clicked "Quit"
    """

    show_requested = Signal()
    quit_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._is_routing = False
        self._setup()

    def _setup(self) -> None:
        self.setIcon(_make_tray_icon("#4f8ef7"))
        self.setToolTip("Virtual Audio Router — Ready")

        menu = QMenu()

        # Title label (non-clickable)
        title_action = menu.addAction("Virtual Audio Router")
        title_action.setEnabled(False)
        menu.addSeparator()

        self._status_action = menu.addAction("Status: Ready")
        self._status_action.setEnabled(False)
        menu.addSeparator()

        show_action = menu.addAction("Show Window")
        show_action.triggered.connect(self.show_requested)

        self._routing_action = menu.addAction("Start Routing")
        self._routing_action.setEnabled(False)  # enabled once devices selected
        menu.addSeparator()

        quit_action = menu.addAction("Quit")
        quit_action.triggered.connect(self.quit_requested)

        self.setContextMenu(menu)
        self.activated.connect(self._on_activated)

    def _on_activated(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.show_requested.emit()

    def set_routing(self, active: bool) -> None:
        self._is_routing = active
        if active:
            self.setIcon(_make_tray_icon("#3ddc84"))
            self.setToolTip("Virtual Audio Router — Routing Active")
            self._status_action.setText("Status: ● Routing")
            self._routing_action.setText("Stop Routing")
        else:
            self.setIcon(_make_tray_icon("#4f8ef7"))
            self.setToolTip("Virtual Audio Router — Ready")
            self._status_action.setText("Status: Ready")
            self._routing_action.setText("Start Routing")

    def set_routing_action_enabled(self, enabled: bool) -> None:
        self._routing_action.setEnabled(enabled)
