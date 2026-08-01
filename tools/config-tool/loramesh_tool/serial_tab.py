"""Tab 3: raw serial monitor — watch the device's live output (boot banner,
'#' log lines, JSON events) to verify the firmware is running correctly."""

from __future__ import annotations

import datetime
import json

import serial
from PySide6.QtCore import QThread, Signal
from PySide6.QtGui import QColor, QFont, QTextCharFormat, QTextCursor
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFileDialog, QHBoxLayout,
                               QLabel, QLineEdit, QMessageBox, QPlainTextEdit,
                               QPushButton, QVBoxLayout, QWidget)

from .protocol import HANDSHAKE
from .ui_common import PortSelector

MAX_LINES = 5000


class SerialReader(QThread):
    line_received = Signal(str)
    error = Signal(str)

    def __init__(self, port: str, baud: int):
        super().__init__()
        self.port = port
        self.baud = baud
        self.ser: serial.Serial | None = None
        self._stop = False

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
        except Exception as e:  # noqa: BLE001
            self.error.emit(str(e))
            return
        buf = bytearray()
        while not self._stop:
            try:
                chunk = self.ser.read(4096)
            except Exception as e:  # noqa: BLE001
                if not self._stop:
                    self.error.emit(f"Verbindung verloren: {e}")
                break
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, _, buf = buf.partition(b"\n")
                text = raw.rstrip(b"\r")
                # Binary data (e.g. gateway MAVLink stream) is summarized
                # instead of garbling the log.
                if any(b < 0x09 or (0x0D < b < 0x20) or b > 0x7E
                       for b in text[:64]):
                    self.line_received.emit(
                        f"[binär, {len(text)} B — MAVLink-Passthrough?]")
                else:
                    self.line_received.emit(text.decode(errors="replace"))
        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass

    def send(self, text: str):
        if self.ser and self.ser.is_open:
            self.ser.write((text + "\n").encode())

    def stop(self):
        self._stop = True


class SerialTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.reader: SerialReader | None = None

        self.ports = PortSelector()
        self.baud = QComboBox()
        self.baud.addItems(["115200", "57600", "921600", "9600"])
        self.btn_connect = QPushButton("Verbinden")
        self.btn_connect.clicked.connect(self._toggle)
        top = QHBoxLayout()
        top.addWidget(self.ports, 1)
        top.addWidget(QLabel("Baud:"))
        top.addWidget(self.baud)
        top.addWidget(self.btn_connect)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(MAX_LINES)
        font = QFont("Consolas")
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.log.setFont(font)

        self.autoscroll = QCheckBox("Autoscroll")
        self.autoscroll.setChecked(True)
        btn_clear = QPushButton("Leeren")
        btn_clear.clicked.connect(self.log.clear)
        btn_save = QPushButton("Log speichern…")
        btn_save.clicked.connect(self._save)
        self.btn_cfg = QPushButton("Konfigmodus")
        self.btn_cfg.setToolTip(f"Sendet den Handshake ({HANDSHAKE})")
        self.btn_cfg.clicked.connect(lambda: self._send_text(HANDSHAKE))
        opts = QHBoxLayout()
        opts.addWidget(self.autoscroll)
        opts.addWidget(btn_clear)
        opts.addWidget(btn_save)
        opts.addWidget(self.btn_cfg)
        opts.addStretch(1)

        self.input = QLineEdit()
        self.input.setPlaceholderText(
            'Zeile senden, z.B. {"cmd":"status"} — Enter zum Senden')
        self.input.returnPressed.connect(self._send_input)

        hint = QLabel(
            "Hinweis: Beim Öffnen des Ports startet das Gerät meist neu (DTR) — "
            "der Bootbanner „# CLMESH …“ erscheint direkt. Danach: „#“ = Log, "
            "JSON-Zeilen = Events/Antworten, alle 30 s eine HB-Lebenszeile.")
        hint.setWordWrap(True)

        lay = QVBoxLayout(self)
        lay.addLayout(top)
        lay.addWidget(hint)
        lay.addWidget(self.log, 1)
        lay.addLayout(opts)
        lay.addWidget(self.input)
        self._set_connected(False)

    # ------------------------------------------------------------------

    def _set_connected(self, on: bool):
        self.input.setEnabled(on)
        self.btn_cfg.setEnabled(on)
        self.btn_connect.setText("Trennen" if on else "Verbinden")

    def _toggle(self):
        if self.reader:
            self.reader.stop()
            self.reader.wait(2000)
            self.reader = None
            self._append("— getrennt —", QColor("#888888"))
            self._set_connected(False)
            return
        port = self.ports.port()
        if not port:
            QMessageBox.warning(self, "Fehler", "Kein COM-Port gewählt.")
            return
        self.reader = SerialReader(port, int(self.baud.currentText()))
        self.reader.line_received.connect(self._on_line)
        self.reader.error.connect(self._on_error)
        self.reader.start()
        self._append(f"— verbunden mit {port} —", QColor("#888888"))
        self._set_connected(True)

    def _on_error(self, msg: str):
        self._append(f"FEHLER: {msg}", QColor("#c62828"))
        if self.reader:
            self.reader.stop()
            self.reader = None
        self._set_connected(False)

    def _line_color(self, line: str) -> QColor | None:
        low = line.lower()
        if "fail" in low or "error" in low or "warnung" in low or "warning" in low:
            return QColor("#c62828")
        if line.startswith("#") or line.startswith("["):
            return QColor("#888888")
        if line.startswith("{"):
            try:
                obj = json.loads(line)
                if "evt" in obj:
                    return QColor("#1565c0")
            except ValueError:
                pass
            return QColor("#2e7d32")
        return None

    def _on_line(self, line: str):
        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self._append(f"[{ts}] {line}", self._line_color(line))

    def _append(self, text: str, color: QColor | None = None):
        cursor = self.log.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        fmt = QTextCharFormat()
        if color:
            fmt.setForeground(color)
        cursor.insertText(text + "\n", fmt)
        if self.autoscroll.isChecked():
            self.log.setTextCursor(cursor)
            self.log.ensureCursorVisible()

    def _send_text(self, text: str):
        if self.reader:
            self.reader.send(text)
            self._append(f">> {text}", QColor("#6a1b9a"))

    def _send_input(self):
        text = self.input.text().strip()
        if text:
            self._send_text(text)
            self.input.clear()

    def _save(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Log speichern", "serial-log.txt", "Text (*.txt)")
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.log.toPlainText())
