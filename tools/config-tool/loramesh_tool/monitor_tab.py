"""Tab 3: live network status via cyclic status polling."""

from __future__ import annotations

import time

from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import (QHBoxLayout, QHeaderView, QLabel, QMessageBox,
                               QPushButton, QTableWidget, QTableWidgetItem,
                               QVBoxLayout, QWidget)

from .config_client import ConfigClient
from .ui_common import PortSelector


class StatusPoller(QThread):
    status = Signal(dict)
    error = Signal(str)

    def __init__(self, port: str, interval_s: float = 2.0):
        super().__init__()
        self.port = port
        self.interval_s = interval_s
        self._stop = False

    def run(self):
        try:
            with ConfigClient(self.port) as c:
                c.enter_config_mode()
                while not self._stop:
                    self.status.emit(c.status())
                    for _ in range(int(self.interval_s * 10)):
                        if self._stop:
                            break
                        time.sleep(0.1)
                c.exit_config()
        except Exception as e:  # noqa: BLE001
            self.error.emit(str(e))

    def stop(self):
        self._stop = True


class MonitorTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.poller = None

        self.ports = PortSelector()
        self.btn = QPushButton("Monitor starten")
        self.btn.clicked.connect(self._toggle)
        top = QHBoxLayout()
        top.addWidget(self.ports, 1)
        top.addWidget(self.btn)

        self.summary = QLabel("—")
        self.counters = QLabel("")

        self.table = QTableWidget(0, 5)
        self.table.setHorizontalHeaderLabels(
            ["Node-ID", "RSSI (dBm)", "SNR (dB)", "Zuletzt gehört", "Pakete"])
        self.table.horizontalHeader().setSectionResizeMode(
            QHeaderView.ResizeMode.Stretch)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)

        lay = QVBoxLayout(self)
        lay.addLayout(top)
        lay.addWidget(QLabel(
            "Hinweis: Während des Monitorings ist ein MAVLink-Gateway im "
            "Konfigmodus (Passthrough pausiert)."))
        lay.addWidget(self.summary)
        lay.addWidget(self.counters)
        lay.addWidget(QLabel("Gehörte Nachbarn:"))
        lay.addWidget(self.table, 1)

    def _toggle(self):
        if self.poller:
            self._stop()
            return
        port = self.ports.port()
        if not port:
            QMessageBox.warning(self, "Fehler", "Kein COM-Port gewählt.")
            return
        self.poller = StatusPoller(port)
        self.poller.status.connect(self._update)
        self.poller.error.connect(self._on_error)
        self.poller.start()
        self.btn.setText("Monitor stoppen")

    def _stop(self):
        if self.poller:
            self.poller.stop()
            self.poller.wait(3000)
            self.poller = None
        self.btn.setText("Monitor starten")

    def _on_error(self, msg: str):
        self._stop()
        QMessageBox.critical(self, "Monitor", msg)

    def _update(self, s: dict):
        radio = "OK" if s.get("radio_ok") else f"FEHLER ({s.get('radio_state')})"
        batt = s.get("batt_mv", 0)
        self.summary.setText(
            f"Node 0x{s.get('node_id', 0):04X} — Rolle {s.get('role')} — "
            f"{s.get('variant')} — Radio {radio} — "
            f"Airtime {s.get('airtime_pct', 0):.1f} % "
            + (f"— Akku {batt / 1000:.2f} V" if batt else ""))
        self.counters.setText(
            f"TX {s.get('tx', 0)} | RX {s.get('rx', 0)} | "
            f"zugestellt {s.get('delivered', 0)} | weitergeleitet {s.get('forwarded', 0)} | "
            f"Duplikate {s.get('dups', 0)} | Auth-Fehler {s.get('auth_fail', 0)} | "
            f"Duty-Drops {s.get('tx_drop_duty', 0)}")

        neighbors = s.get("neighbors", [])
        self.table.setRowCount(len(neighbors))
        for row, n in enumerate(neighbors):
            self.table.setItem(row, 0, QTableWidgetItem(f"0x{n.get('id', 0):04X}"))
            self.table.setItem(row, 1, QTableWidgetItem(f"{n.get('rssi', 0):.0f}"))
            self.table.setItem(row, 2, QTableWidgetItem(f"{n.get('snr', 0):.1f}"))
            self.table.setItem(row, 3, QTableWidgetItem(f"vor {n.get('age_s', 0)} s"))
            self.table.setItem(row, 4, QTableWidgetItem(str(n.get("packets", 0))))
