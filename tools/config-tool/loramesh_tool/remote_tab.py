"""Tab 5: remote management of mesh devices through a USB-attached bridge
node. The local device relays {"cmd":"remote"} requests over the air
(TOPIC_ADMIN, admin PSK); responses come back as admin events."""

from __future__ import annotations

import json

from PySide6.QtWidgets import (QCheckBox, QComboBox, QDialog,
                               QDialogButtonBox, QDoubleSpinBox, QFormLayout,
                               QHBoxLayout, QHeaderView, QLabel, QMessageBox,
                               QPushButton, QSpinBox, QTableWidget,
                               QTableWidgetItem, QTextEdit, QVBoxLayout,
                               QWidget)

from . import protocol
from .config_client import ConfigClient
from .ui_common import PortSelector, Worker

ROLE_LABELS = {"router": "Router", "node": "Node",
               "mavlink_gateway": "MAVLink-Gateway"}


class RemoteConfigDialog(QDialog):
    """Compact editor for remotely settable radio/mesh parameters."""

    def __init__(self, node_id: int, cfg: dict, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Konfiguration Node 0x{node_id:04X}")

        self.freq = QDoubleSpinBox()
        self.freq.setRange(863.0, 870.0)
        self.freq.setDecimals(3)
        self.freq.setSingleStep(0.025)
        self.freq.setValue(cfg.get("freq", 869.525))
        self.sf = QSpinBox()
        self.sf.setRange(5, 12)
        self.sf.setValue(cfg.get("sf", 7))
        self.bw = QComboBox()
        self.bw.addItems(["125", "250", "500"])
        self.bw.setCurrentText(str(int(cfg.get("bw", 125))))
        self.cr = QComboBox()
        self.cr.addItems(["5", "6", "7", "8"])
        self.cr.setCurrentText(str(cfg.get("cr", 5)))
        self.tx_dbm = QSpinBox()
        self.tx_dbm.setRange(-9, 27)
        self.tx_dbm.setValue(cfg.get("tx_dbm", 14))
        self.hops = QSpinBox()
        self.hops.setRange(0, 7)
        self.hops.setValue(cfg.get("hops", 3))
        self.relay = QCheckBox("Relay aktiv (Node leitet fremde Pakete weiter)")
        self.relay.setChecked(cfg.get("relay", False))
        self.wx_int = QSpinBox()
        self.wx_int.setRange(5, 3600)
        self.wx_int.setValue(cfg.get("weather_interval", 60))
        self.pos_int = QSpinBox()
        self.pos_int.setRange(1, 3600)
        self.pos_int.setValue(cfg.get("position_interval", 10))

        self.check = QLabel("")
        for w in (self.freq, self.sf, self.tx_dbm):
            w.valueChanged.connect(self._validate)
        self.bw.currentTextChanged.connect(self._validate)
        self.cr.currentTextChanged.connect(self._validate)

        warn = QLabel(
            "⚠ Falsche Funkparameter trennen das Gerät vom Netz — danach "
            "hilft nur noch USB-Zugriff vor Ort! Das Gerät startet nach dem "
            "Schreiben automatisch neu.")
        warn.setWordWrap(True)
        warn.setStyleSheet("color: #c62828;")

        form = QFormLayout()
        form.addRow("Frequenz (MHz):", self.freq)
        form.addRow("Spreading Factor:", self.sf)
        form.addRow("Bandbreite (kHz):", self.bw)
        form.addRow("Coding Rate (4/x):", self.cr)
        form.addRow("TX-Leistung (dBm):", self.tx_dbm)
        form.addRow("Hop-Limit:", self.hops)
        form.addRow("", self.relay)
        form.addRow("Wetter-Intervall (s):", self.wx_int)
        form.addRow("Positions-Intervall (s):", self.pos_int)
        form.addRow("EU868-Prüfung:", self.check)

        btns = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                QDialogButtonBox.StandardButton.Cancel)
        btns.accepted.connect(self._accept_checked)
        btns.rejected.connect(self.reject)

        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addWidget(warn)
        lay.addWidget(btns)
        self._validate()

    def _validate(self, *_):
        ok, msg = protocol.validate_phy(
            self.freq.value(), self.sf.value(), float(self.bw.currentText()),
            int(self.cr.currentText()), self.tx_dbm.value())
        self.check.setText(("✔ " if ok else "✘ ") + msg)
        self._ok = ok

    def _accept_checked(self):
        if not self._ok:
            QMessageBox.warning(self, "EU868", self.check.text())
            return
        self.accept()

    def values(self) -> dict:
        return {
            "acmd": "set",
            "freq": round(self.freq.value(), 3),
            "sf": self.sf.value(),
            "bw": float(self.bw.currentText()),
            "cr": int(self.cr.currentText()),
            "tx_dbm": self.tx_dbm.value(),
            "hops": self.hops.value(),
            "relay": self.relay.isChecked(),
            "weather_interval": self.wx_int.value(),
            "position_interval": self.pos_int.value(),
        }


class RemoteTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.client: ConfigClient | None = None
        self.worker = None

        self.ports = PortSelector()
        self.btn_connect = QPushButton("Brücken-Node verbinden")
        self.btn_connect.clicked.connect(self._toggle_connect)
        top = QHBoxLayout()
        top.addWidget(QLabel("USB-Node:"))
        top.addWidget(self.ports, 1)
        top.addWidget(self.btn_connect)

        self.info = QLabel(
            "Ein per USB angeschlossener Node (mit Admin-PSK) dient als "
            "Funk-Brücke ins Mesh.")
        self.info.setWordWrap(True)

        self.btn_scan = QPushButton("Netzwerk scannen")
        self.btn_scan.clicked.connect(self._scan)
        self.btn_status = QPushButton("Status")
        self.btn_status.clicked.connect(lambda: self._simple_cmd("status"))
        self.btn_get = QPushButton("Konfig lesen")
        self.btn_get.clicked.connect(lambda: self._simple_cmd("get"))
        self.btn_edit = QPushButton("Konfig ändern…")
        self.btn_edit.clicked.connect(self._edit_config)
        self.btn_reboot = QPushButton("Neustart")
        self.btn_reboot.clicked.connect(self._reboot)
        self.btn_factory = QPushButton("Werksreset")
        self.btn_factory.clicked.connect(self._factory)
        actions = QHBoxLayout()
        for b in (self.btn_scan, self.btn_status, self.btn_get, self.btn_edit,
                  self.btn_reboot, self.btn_factory):
            actions.addWidget(b)
        actions.addStretch(1)

        self.table = QTableWidget(0, 7)
        self.table.setHorizontalHeaderLabels(
            ["Node-ID", "Rolle", "FW", "RSSI*", "SNR*", "Uptime", "Akku"])
        self.table.horizontalHeader().setSectionResizeMode(
            QHeaderView.ResizeMode.Stretch)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)

        self.output = QTextEdit()
        self.output.setReadOnly(True)

        lay = QVBoxLayout(self)
        lay.addLayout(top)
        lay.addWidget(self.info)
        lay.addLayout(actions)
        lay.addWidget(QLabel(
            "Gefundene Geräte (* = Empfangsqualität des Kommandos aus "
            "Gerätesicht):"))
        lay.addWidget(self.table, 1)
        lay.addWidget(QLabel("Antwort:"))
        lay.addWidget(self.output, 1)
        self._set_ready(False)

    # ------------------------------------------------------------------

    def _set_ready(self, on: bool):
        for b in (self.btn_scan, self.btn_status, self.btn_get, self.btn_edit,
                  self.btn_reboot, self.btn_factory):
            b.setEnabled(on)

    def _busy(self, on: bool):
        self._set_ready(not on and self.client is not None)
        self.btn_connect.setEnabled(not on)

    def _selected_node(self) -> int | None:
        row = self.table.currentRow()
        if row < 0:
            QMessageBox.information(
                self, "Hinweis", "Zuerst ein Gerät in der Tabelle auswählen "
                "(ggf. „Netzwerk scannen“).")
            return None
        return int(self.table.item(row, 0).text(), 16)

    def _run(self, fn, on_result):
        self._busy(True)

        def done(result):
            self._busy(False)
            on_result(result)

        def fail(msg):
            self._busy(False)
            QMessageBox.critical(self, "Fehler", msg)

        self.worker = Worker(fn)
        self.worker.result.connect(done)
        self.worker.error.connect(fail)
        self.worker.start()

    # ------------------------------------------------------------------

    def _toggle_connect(self):
        if self.client:
            self.client.close()
            self.client = None
            self.btn_connect.setText("Brücken-Node verbinden")
            self.info.setText("Getrennt.")
            self._set_ready(False)
            return
        port = self.ports.port()
        if not port:
            QMessageBox.warning(self, "Fehler", "Kein COM-Port gewählt.")
            return

        def task():
            c = ConfigClient(port)
            c.enter_config_mode()
            cfg = c.get_config()
            return c, cfg

        def done(result):
            self.client, cfg = result
            if not cfg.get("has_admin_psk"):
                self.info.setText(
                    "⚠ Brücken-Node hat KEINEN Admin-PSK — Fernverwaltung "
                    "nicht möglich. Im Tab „Konfiguration“ setzen.")
            else:
                self.info.setText(
                    f"Brücke: Node 0x{cfg.get('node_id', 0):04X} "
                    f"({cfg.get('variant', '?')}) auf {port}.")
                self._set_ready(True)
            self.btn_connect.setText("Trennen")

        self._run(task, done)

    def _scan(self):
        client = self.client

        def task():
            return client.discover(duration_s=8.0)

        def done(found: dict):
            self.table.setRowCount(len(found))
            for row, (nid, d) in enumerate(sorted(found.items())):
                batt = d.get("batt_mv", 0)
                cells = [
                    f"0x{nid:04X}",
                    ROLE_LABELS.get(d.get("role", ""), d.get("role", "?")),
                    str(d.get("fw", "?")),
                    f"{d.get('rssi', 0):.0f} dBm",
                    f"{d.get('snr', 0):.1f} dB",
                    f"{d.get('uptime_s', 0)} s",
                    f"{batt / 1000:.2f} V" if batt else "—",
                ]
                for col, text in enumerate(cells):
                    self.table.setItem(row, col, QTableWidgetItem(text))
            self.output.setPlainText(
                f"{len(found)} Gerät(e) haben auf den Broadcast-Ping "
                "geantwortet.")

        self._run(task, done)

    def _show_response(self, resp: dict):
        self.output.setPlainText(json.dumps(resp, indent=2, ensure_ascii=False))

    def _simple_cmd(self, acmd: str):
        dst = self._selected_node()
        if dst is None:
            return
        client = self.client
        self._run(lambda: client.remote(dst, {"acmd": acmd}),
                  self._show_response)

    def _edit_config(self):
        dst = self._selected_node()
        if dst is None:
            return
        client = self.client

        def fetch():
            return client.remote(dst, {"acmd": "get"})

        def got_config(cfg: dict):
            if not cfg.get("ok"):
                self._show_response(cfg)
                return
            dlg = RemoteConfigDialog(dst, cfg, self)
            if dlg.exec() != QDialog.DialogCode.Accepted:
                return
            values = dlg.values()

            def apply():
                resp = client.remote(dst, values)
                if resp.get("ok"):
                    # Radio params apply on boot — trigger the reboot now.
                    client.remote(dst, {"acmd": "reboot"}, retries=1)
                return resp

            self._run(apply, lambda r: self._show_response(
                {**r, "hinweis": "Gerät startet neu und meldet sich mit den "
                                 "neuen Parametern."}))

        self._run(fetch, got_config)

    def _reboot(self):
        dst = self._selected_node()
        if dst is None:
            return
        client = self.client
        self._run(lambda: client.remote(dst, {"acmd": "reboot"}, retries=1),
                  self._show_response)

    def _factory(self):
        dst = self._selected_node()
        if dst is None:
            return
        if QMessageBox.question(
                self, "Werksreset",
                f"Node 0x{dst:04X} wirklich auf Werkszustand zurücksetzen?\n"
                "Konfiguration UND Schlüssel werden gelöscht — danach ist das "
                "Gerät nur noch per USB erreichbar!"
        ) != QMessageBox.StandardButton.Yes:
            return
        client = self.client
        self._run(lambda: client.remote(dst, {"acmd": "factory"}, retries=1),
                  self._show_response)
