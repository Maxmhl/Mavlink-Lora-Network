"""Tab 2: read/write node configuration over the serial config protocol."""

from __future__ import annotations

from PySide6.QtWidgets import (QCheckBox, QComboBox, QDoubleSpinBox,
                               QFormLayout, QGroupBox, QHBoxLayout, QLabel,
                               QLineEdit, QMessageBox, QPushButton, QSpinBox,
                               QVBoxLayout, QWidget)

from . import protocol
from .config_client import ConfigClient
from .ui_common import PortSelector, Worker


class ConfigTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.worker = None

        self.ports = PortSelector()
        self.btn_connect = QPushButton("Verbinden && Auslesen")
        self.btn_connect.clicked.connect(self._connect)
        top = QHBoxLayout()
        top.addWidget(self.ports, 1)
        top.addWidget(self.btn_connect)

        self.info = QLabel("Nicht verbunden.")

        # --- Rolle / Identität ---
        self.role = QComboBox()
        self.role.addItems(["node", "mavlink_gateway", "router"])
        self.role.currentTextChanged.connect(self._role_changed)
        self.node_id = QSpinBox()
        self.node_id.setRange(1, 0xFFFE)
        ident = QFormLayout()
        ident.addRow("Rolle:", self.role)
        ident.addRow("Node-ID:", self.node_id)
        g_ident = QGroupBox("Gerät")
        g_ident.setLayout(ident)

        # --- LoRa PHY ---
        self.freq = QDoubleSpinBox()
        self.freq.setRange(863.0, 870.0)
        self.freq.setDecimals(3)
        self.freq.setSingleStep(0.025)
        self.freq.setValue(869.525)
        self.sf = QSpinBox()
        self.sf.setRange(5, 12)
        self.sf.setValue(7)
        self.bw = QComboBox()
        self.bw.addItems(["125", "250", "500"])
        self.cr = QComboBox()
        self.cr.addItems(["5", "6", "7", "8"])
        self.tx_dbm = QSpinBox()
        self.tx_dbm.setRange(-9, 27)
        self.tx_dbm.setValue(14)
        self.sync = QSpinBox()
        self.sync.setRange(0, 255)
        self.sync.setValue(0x2B)
        self.sync.setDisplayIntegerBase(16)
        self.sync.setPrefix("0x")
        self.hops = QSpinBox()
        self.hops.setRange(0, 7)
        self.hops.setValue(3)
        self.relay = QCheckBox("Node leitet fremde Pakete weiter (Relay)")
        self.phy_check = QLabel("")
        for w in (self.freq, self.sf, self.tx_dbm):
            w.valueChanged.connect(self._validate_phy)
        self.bw.currentTextChanged.connect(self._validate_phy)
        self.cr.currentTextChanged.connect(self._validate_phy)

        phy = QFormLayout()
        phy.addRow("Frequenz (MHz):", self.freq)
        phy.addRow("Spreading Factor:", self.sf)
        phy.addRow("Bandbreite (kHz):", self.bw)
        phy.addRow("Coding Rate (4/x):", self.cr)
        phy.addRow("TX-Leistung (dBm):", self.tx_dbm)
        phy.addRow("Sync-Word (Netz-ID):", self.sync)
        phy.addRow("Hop-Limit:", self.hops)
        phy.addRow("", self.relay)
        phy.addRow("EU868-Prüfung:", self.phy_check)
        g_phy = QGroupBox("LoRa-Parameter (EU868)")
        g_phy.setLayout(phy)

        # --- Topics ---
        self.tx_boxes = {n: QCheckBox(n) for n in ("mavlink", "weather", "position")}
        self.rx_boxes = {n: QCheckBox(n)
                         for n in ("mavlink", "weather", "position", "generic")}
        tx_row = QHBoxLayout()
        for b in self.tx_boxes.values():
            tx_row.addWidget(b)
        rx_row = QHBoxLayout()
        for b in self.rx_boxes.values():
            rx_row.addWidget(b)
        topics = QFormLayout()
        topics.addRow("Senden:", tx_row)
        topics.addRow("Empfangen:", rx_row)
        g_topics = QGroupBox("Datentypen (Topics)")
        g_topics.setLayout(topics)

        # --- MAVLink ---
        self.mav_peer = QSpinBox()
        self.mav_peer.setRange(0, 0xFFFF)
        self.mav_peer.setValue(0xFFFF)
        self.mav_peer.setToolTip("65535 = Broadcast an alle MAVLink-Abonnenten")
        self.mav_baud = QComboBox()
        self.mav_baud.addItems(["57600", "115200", "38400", "230400"])
        mav = QFormLayout()
        mav.addRow("Ziel-Node-ID (Peer):", self.mav_peer)
        mav.addRow("FC-UART-Baud:", self.mav_baud)
        g_mav = QGroupBox("MAVLink")
        g_mav.setLayout(mav)

        # --- PSK / Admin-PSK ---
        self.psk = QLineEdit()
        self.psk.setPlaceholderText("32 Hex-Zeichen — leer lassen = unverändert")
        self.psk.setMaxLength(32)
        self.btn_psk_gen = QPushButton("Generieren")
        self.btn_psk_gen.clicked.connect(
            lambda: self.psk.setText(protocol.generate_psk()))
        psk_row = QHBoxLayout()
        psk_row.addWidget(self.psk, 1)
        psk_row.addWidget(self.btn_psk_gen)
        self.psk_state = QLabel("")

        # Second key for remote management (TOPIC_ADMIN) — every role gets
        # this one, including routers (which never get the data PSK).
        self.admin_psk = QLineEdit()
        self.admin_psk.setPlaceholderText(
            "32 Hex-Zeichen — leer lassen = unverändert")
        self.admin_psk.setMaxLength(32)
        btn_agen = QPushButton("Generieren")
        btn_agen.clicked.connect(
            lambda: self.admin_psk.setText(protocol.generate_psk()))
        apsk_row = QHBoxLayout()
        apsk_row.addWidget(self.admin_psk, 1)
        apsk_row.addWidget(btn_agen)
        self.admin_psk_state = QLabel("")

        psk_form = QFormLayout()
        psk_form.addRow("Netzwerk-PSK:", psk_row)
        psk_form.addRow("", self.psk_state)
        psk_form.addRow("Admin-PSK (Fernverwaltung):", apsk_row)
        psk_form.addRow("", self.admin_psk_state)
        self.g_psk = QGroupBox("Verschlüsselung (AES-128-GCM, Ende-zu-Ende)")
        self.g_psk.setLayout(psk_form)

        # --- Aktionen ---
        self.btn_write = QPushButton("Auf Gerät schreiben")
        self.btn_write.clicked.connect(self._write)
        self.btn_reboot = QPushButton("Neustart")
        self.btn_reboot.clicked.connect(self._reboot)
        actions = QHBoxLayout()
        actions.addWidget(self.btn_write)
        actions.addWidget(self.btn_reboot)
        actions.addStretch(1)

        lay = QVBoxLayout(self)
        lay.addLayout(top)
        lay.addWidget(self.info)
        lay.addWidget(g_ident)
        lay.addWidget(g_phy)
        lay.addWidget(g_topics)
        lay.addWidget(g_mav)
        lay.addWidget(self.g_psk)
        lay.addLayout(actions)
        lay.addStretch(1)
        self._set_connected(False)
        self._validate_phy()

    # ------------------------------------------------------------------

    def _set_connected(self, on: bool):
        for w in (self.btn_write, self.btn_reboot):
            w.setEnabled(on)

    def _role_changed(self, role: str):
        # Routers forward ciphertext only — they must not get the data PSK.
        # The admin PSK stays available on every role (remote management).
        is_router = role == "router"
        self.psk.setEnabled(not is_router)
        self.btn_psk_gen.setEnabled(not is_router)
        if is_router:
            self.psk.clear()

    def _validate_phy(self, *_):
        ok, msg = protocol.validate_phy(
            self.freq.value(), self.sf.value(), float(self.bw.currentText()),
            int(self.cr.currentText()), self.tx_dbm.value())
        self.phy_check.setText(("✔ " if ok else "✘ ") + msg)
        self.phy_check.setStyleSheet(
            "color: #2e7d32;" if ok else "color: #c62828; font-weight: bold;")
        self._phy_ok = ok

    def _run(self, fn, on_result):
        self.btn_connect.setEnabled(False)
        self.worker = Worker(fn)
        self.worker.result.connect(lambda r: (self.btn_connect.setEnabled(True),
                                              on_result(r)))
        self.worker.error.connect(self._error)
        self.worker.start()

    def _error(self, msg: str):
        self.btn_connect.setEnabled(True)
        QMessageBox.critical(self, "Fehler", msg)

    # ------------------------------------------------------------------

    def _connect(self):
        port = self.ports.port()
        if not port:
            QMessageBox.warning(self, "Fehler", "Kein COM-Port gewählt.")
            return

        def task():
            with ConfigClient(port) as c:
                c.enter_config_mode()
                return c.get_config()

        self._run(task, self._apply_config)

    def _apply_config(self, cfg: dict):
        self.info.setText(
            f"Verbunden: {cfg.get('variant', '?')} — FW {cfg.get('fw', '?')} — "
            f"Node 0x{cfg.get('node_id', 0):04X}"
            + ("" if cfg.get("configured") else " — (unkonfiguriert)"))
        self.role.setCurrentText(cfg.get("role", "node"))
        self.node_id.setValue(cfg.get("node_id", 1))
        self.freq.setValue(cfg.get("freq", 869.525))
        self.sf.setValue(cfg.get("sf", 7))
        self.bw.setCurrentText(str(int(cfg.get("bw", 125))))
        self.cr.setCurrentText(str(cfg.get("cr", 5)))
        self.tx_dbm.setValue(cfg.get("tx_dbm", 14))
        self.sync.setValue(cfg.get("sync", 0x2B))
        self.hops.setValue(cfg.get("hops", 3))
        self.relay.setChecked(cfg.get("relay", False))
        tx_mask = cfg.get("tx_topics", 0)
        rx_mask = cfg.get("rx_topics", 0)
        for name, box in self.tx_boxes.items():
            box.setChecked(bool(tx_mask & protocol.topic_bit(protocol.TOPICS[name])))
        for name, box in self.rx_boxes.items():
            if name == "generic":
                box.setChecked(bool(rx_mask & protocol.topic_bit(protocol.TOPICS["generic"])))
            else:
                box.setChecked(bool(rx_mask & protocol.topic_bit(protocol.TOPICS[name])))
        self.mav_peer.setValue(cfg.get("mav_peer", 0xFFFF))
        self.mav_baud.setCurrentText(str(cfg.get("mav_baud", 57600)))
        self.psk.clear()
        self.psk_state.setText(
            "PSK gesetzt ✔" if cfg.get("has_psk") else
            "KEIN PSK gesetzt — Payloads unverschlüsselt!")
        self.admin_psk.clear()
        self.admin_psk_state.setText(
            "Admin-PSK gesetzt ✔" if cfg.get("has_admin_psk") else
            "Kein Admin-PSK — Gerät ist nicht fernverwaltbar.")
        self._set_connected(True)

    def _collect(self) -> dict:
        tx_mask = 0
        for name, box in self.tx_boxes.items():
            if box.isChecked():
                tx_mask |= protocol.topic_bit(protocol.TOPICS[name])
        rx_mask = 0
        for name, box in self.rx_boxes.items():
            if box.isChecked():
                rx_mask |= protocol.topic_bit(protocol.TOPICS[name])
        values = {
            "role": self.role.currentText(),
            "node_id": self.node_id.value(),
            "freq": round(self.freq.value(), 3),
            "bw": float(self.bw.currentText()),
            "sf": self.sf.value(),
            "cr": int(self.cr.currentText()),
            "tx_dbm": self.tx_dbm.value(),
            "sync": self.sync.value(),
            "hops": self.hops.value(),
            "relay": self.relay.isChecked(),
            "tx_topics": tx_mask,
            "rx_topics": rx_mask,
            "mav_peer": self.mav_peer.value(),
            "mav_baud": int(self.mav_baud.currentText()),
        }
        psk = self.psk.text().strip().lower()
        if psk and self.role.currentText() != "router":
            values["psk"] = psk
        admin_psk = self.admin_psk.text().strip().lower()
        if admin_psk:
            values["admin_psk"] = admin_psk
        return values

    def _write(self):
        if not self._phy_ok:
            QMessageBox.warning(self, "EU868", self.phy_check.text())
            return
        for label, text in (("PSK", self.psk.text().strip()),
                            ("Admin-PSK", self.admin_psk.text().strip())):
            if text and (len(text) != 32 or
                         any(c not in "0123456789abcdefABCDEF" for c in text)):
                QMessageBox.warning(
                    self, label, f"{label} muss genau 32 Hex-Zeichen haben.")
                return
        port = self.ports.port()
        values = self._collect()

        def task():
            with ConfigClient(port) as c:
                c.enter_config_mode()
                resp = c.set_config(values)
                c.reboot()
                return resp

        self._run(task, lambda r: (
            self.info.setText("Konfiguration geschrieben — Gerät startet neu."),
            QMessageBox.information(
                self, "OK",
                "Konfiguration gespeichert."
                + (" (Werte wurden auf EU868-Limits begrenzt!)"
                   if r.get("clamped") else ""))))

    def _reboot(self):
        port = self.ports.port()

        def task():
            with ConfigClient(port) as c:
                c.enter_config_mode()
                c.reboot()
                return True

        self._run(task, lambda _:
                  self.info.setText("Neustart ausgelöst."))
