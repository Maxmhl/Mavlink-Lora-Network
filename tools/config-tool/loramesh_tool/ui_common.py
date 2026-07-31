"""Small shared Qt helpers."""

from __future__ import annotations

from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import QComboBox, QHBoxLayout, QPushButton, QWidget

from .ports import scan_ports


class Worker(QThread):
    """Run a callable off the UI thread; emit its result or error string."""

    result = Signal(object)
    error = Signal(str)

    def __init__(self, fn, parent=None):
        super().__init__(parent)
        self._fn = fn

    def run(self):
        try:
            self.result.emit(self._fn())
        except Exception as e:  # noqa: BLE001 — shown in the UI
            self.error.emit(str(e))


class PortSelector(QWidget):
    """COM-port combo box with a refresh button."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.combo = QComboBox()
        self.combo.setMinimumWidth(280)
        btn = QPushButton("⟳")
        btn.setFixedWidth(32)
        btn.setToolTip("Ports neu einlesen")
        btn.clicked.connect(self.refresh)
        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(self.combo, 1)
        lay.addWidget(btn)
        self.refresh()

    def refresh(self):
        current = self.port()
        self.combo.clear()
        for p in scan_ports():
            self.combo.addItem(p.label, p.device)
        if current:
            idx = self.combo.findData(current)
            if idx >= 0:
                self.combo.setCurrentIndex(idx)

    def port(self) -> str | None:
        return self.combo.currentData()
