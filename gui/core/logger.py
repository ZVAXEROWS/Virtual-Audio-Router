"""
logger.py — Virtual Audio Router
==================================
Python-side log consumer that polls the C++ engine ring buffer
and feeds it into Python's standard logging system.

A QTimer in the main window drives periodic polling (every 500ms).
The GUI log panel subscribes to Python logging via a custom Handler.
"""

from __future__ import annotations
import logging
from PySide6.QtCore import QObject, Signal

# Map C++ LogLevel int to Python logging level
_LEVEL_MAP = {
    0: logging.DEBUG,
    1: logging.INFO,
    2: logging.WARNING,
    3: logging.ERROR,
    4: logging.CRITICAL,
}

_LEVEL_NAME = {
    0: "DEBUG",
    1: "INFO",
    2: "WARN",
    3: "ERROR",
    4: "FATAL",
}

py_logger = logging.getLogger("var.engine")


class EngineLogPoller(QObject):
    """
    Polls the C++ engine ring buffer for new log entries and emits them
    as Qt signals so the log panel can display them without blocking the UI.
    """

    new_log_entry = Signal(dict)   # emits each new log entry dict

    def __init__(self, engine_bridge, parent=None):
        super().__init__(parent)
        self._bridge = engine_bridge
        self._last_ts: int = -1

    def poll(self) -> None:
        """Called periodically by QTimer. Fetches new log entries from C++ engine."""
        entries = self._bridge.get_recent_logs(200)
        for entry in entries:
            ts = entry.get("timestamp_ms", 0)
            if ts > self._last_ts:
                self._last_ts = ts
                self.new_log_entry.emit(entry)
                # Also route through Python logging
                level = _LEVEL_MAP.get(entry.get("level", 1), logging.INFO)
                py_logger.log(level, "[%s] %s",
                              entry.get("source", "engine"),
                              entry.get("message", ""))


class QtLogHandler(logging.Handler):
    """
    Python logging Handler that emits to a Qt signal.
    Connect signal to the log panel's append slot.
    """

    class _Emitter(QObject):
        log_record = Signal(str, int)  # message, level

    def __init__(self):
        super().__init__()
        self.emitter = self._Emitter()

    def emit(self, record: logging.LogRecord) -> None:
        msg = self.format(record)
        self.emitter.log_record.emit(msg, record.levelno)
