"""Tab 4: identify the MAVLink gateway COM port for Mission Planner / QGC."""

from __future__ import annotations

from PySide6.QtGui import QGuiApplication
from PySide6.QtWidgets import (QHBoxLayout, QHeaderView, QLabel, QPushButton,
                               QTableWidget, QTableWidgetItem, QVBoxLayout,
                               QWidget)

from .config_client import probe_port
from .ports import PortInfo, scan_ports
from .ui_common import Worker


class GatewayTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.worker = None
        self.gateway_port: str | None = None

        self.btn_scan = QPushButton("Geräte identifizieren")
        self.btn_scan.clicked.connect(self._scan)
        self.btn_copy = QPushButton("Gateway-Port kopieren")
        self.btn_copy.clicked.connect(self._copy)
        self.btn_copy.setEnabled(False)
        top = QHBoxLayout()
        top.addWidget(self.btn_scan)
        top.addWidget(self.btn_copy)
        top.addStretch(1)

        self.result = QLabel("Noch nicht gesucht.")
        self.result.setWordWrap(True)
        self.result.setStyleSheet("font-size: 14pt;")

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(
            ["COM-Port", "USB-Chip / Beschreibung", "Rolle", "Node-ID"])
        self.table.horizontalHeader().setSectionResizeMode(
            QHeaderView.ResizeMode.Stretch)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)

        hint = QLabel(
            "<b>Nutzung in der GCS:</b> Den markierten Gateway-Port in "
            "Mission Planner oben rechts als „Serial“-Verbindung wählen "
            "(Baud 115200) bzw. in QGroundControl unter Kommunikationsverbindungen "
            "einen seriellen Link anlegen. Wichtig: Dieses Tool muss den Port "
            "vorher freigeben — Monitor/Konfiguration schließen, die GCS braucht "
            "Exklusivzugriff.")
        hint.setWordWrap(True)

        lay = QVBoxLayout(self)
        lay.addLayout(top)
        lay.addWidget(self.result)
        lay.addWidget(self.table, 1)
        lay.addWidget(hint)

    def _scan(self):
        self.btn_scan.setEnabled(False)
        self.result.setText("Suche läuft — Geräte werden kurz neu gestartet…")

        def task() -> list[PortInfo]:
            infos = []
            for info in scan_ports():
                try:
                    infos.append(probe_port(info))
                except Exception:
                    info.role = None  # no CLMESH device (or port busy)
                    infos.append(info)
            return infos

        self.worker = Worker(task)
        self.worker.result.connect(self._show)
        self.worker.error.connect(lambda e: (self.btn_scan.setEnabled(True),
                                             self.result.setText(f"Fehler: {e}")))
        self.worker.start()

    def _show(self, infos: list[PortInfo]):
        self.btn_scan.setEnabled(True)
        self.table.setRowCount(len(infos))
        self.gateway_port = None
        for row, info in enumerate(infos):
            role = info.role or "—"
            if info.role == "mavlink_gateway":
                role = "★ MAVLINK-GATEWAY"
                self.gateway_port = info.device
            self.table.setItem(row, 0, QTableWidgetItem(info.device))
            self.table.setItem(row, 1, QTableWidgetItem(
                " / ".join(x for x in (info.bridge, info.description) if x)))
            self.table.setItem(row, 2, QTableWidgetItem(role))
            self.table.setItem(row, 3, QTableWidgetItem(
                f"0x{info.node_id:04X}" if info.node_id is not None else "—"))

        if self.gateway_port:
            self.result.setText(
                f"✔ MAVLink-Gateway aktiv auf <b>{self.gateway_port}</b> — "
                f"diesen Port in Mission Planner / QGroundControl auswählen.")
            self.btn_copy.setEnabled(True)
        else:
            self.result.setText(
                "Kein MAVLink-Gateway gefunden. Ist ein Gerät mit Rolle "
                "„mavlink_gateway“ angeschlossen?")
            self.btn_copy.setEnabled(False)

    def _copy(self):
        if self.gateway_port:
            QGuiApplication.clipboard().setText(self.gateway_port)
