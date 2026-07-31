"""Tab 1: flash firmware (merged image) via esptool."""

from __future__ import annotations

from PySide6.QtWidgets import (QComboBox, QFileDialog, QFormLayout, QHBoxLayout,
                               QLabel, QLineEdit, QMessageBox, QPushButton,
                               QTextEdit, QVBoxLayout, QWidget)

from .flasher import BOARD_PROFILES, FlashWorker
from .ui_common import PortSelector


class FlashTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.worker = None

        self.ports = PortSelector()
        self.board = QComboBox()
        self.board.addItems(BOARD_PROFILES.keys())
        self.baud = QComboBox()
        self.baud.addItems(["460800", "921600", "115200"])

        self.fw_path = QLineEdit()
        self.fw_path.setPlaceholderText(
            "firmware-merged.bin  (aus firmware/.pio/build/<env>/)")
        browse = QPushButton("Durchsuchen…")
        browse.clicked.connect(self._browse)
        path_row = QHBoxLayout()
        path_row.addWidget(self.fw_path, 1)
        path_row.addWidget(browse)

        form = QFormLayout()
        form.addRow("COM-Port:", self.ports)
        form.addRow("Board:", self.board)
        form.addRow("Baudrate:", self.baud)
        form.addRow("Firmware:", path_row)

        self.btn_flash = QPushButton("Firmware flashen")
        self.btn_flash.clicked.connect(self._flash)
        self.btn_erase = QPushButton("Flash löschen (Factory Reset)")
        self.btn_erase.clicked.connect(self._erase)
        btns = QHBoxLayout()
        btns.addWidget(self.btn_flash)
        btns.addWidget(self.btn_erase)
        btns.addStretch(1)

        self.log = QTextEdit()
        self.log.setReadOnly(True)

        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addLayout(btns)
        lay.addWidget(QLabel(
            "Hinweis: Das Merged-Image wird bei Offset 0x0 geschrieben. "
            "Nach dem Flashen die Rolle im Tab „Konfiguration“ setzen."))
        lay.addWidget(self.log, 1)

    def _browse(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Firmware-Image wählen", "", "Firmware (*.bin)")
        if path:
            self.fw_path.setText(path)

    def _profile(self):
        return BOARD_PROFILES[self.board.currentText()]

    def _start(self, erase: bool):
        port = self.ports.port()
        if not port:
            QMessageBox.warning(self, "Fehler", "Kein COM-Port gewählt.")
            return
        if not erase and not self.fw_path.text():
            QMessageBox.warning(self, "Fehler", "Keine Firmware-Datei gewählt.")
            return
        self.btn_flash.setEnabled(False)
        self.btn_erase.setEnabled(False)
        self.log.clear()
        self.worker = FlashWorker(
            port, self._profile()["chip"], self.fw_path.text(),
            baud=int(self.baud.currentText()), erase=erase)
        self.worker.progress.connect(self.log.append)
        self.worker.finished_ok.connect(lambda: self._done(None))
        self.worker.failed.connect(self._done)
        self.worker.start()

    def _flash(self):
        self._start(erase=False)

    def _erase(self):
        if QMessageBox.question(
                self, "Flash löschen",
                "Kompletten Flash inkl. Konfiguration und PSK löschen?"
        ) == QMessageBox.StandardButton.Yes:
            self._start(erase=True)

    def _done(self, err: str | None):
        self.btn_flash.setEnabled(True)
        self.btn_erase.setEnabled(True)
        if err:
            self.log.append(f"\nFEHLER: {err}")
            QMessageBox.critical(self, "Fehlgeschlagen", err)
        else:
            self.log.append("\nFertig.")
