"""Application entry point."""

from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget

from . import __version__
from .config_tab import ConfigTab
from .flash_tab import FlashTab
from .gateway_tab import GatewayTab
from .monitor_tab import MonitorTab
from .remote_tab import RemoteTab
from .serial_tab import SerialTab


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(f"LoRa-Mesh Konfigurator v{__version__}")
        self.resize(860, 720)

        tabs = QTabWidget()
        tabs.addTab(FlashTab(), "1. Flashen")
        tabs.addTab(ConfigTab(), "2. Konfiguration")
        tabs.addTab(SerialTab(), "3. Serial-Monitor")
        tabs.addTab(MonitorTab(), "4. Netzwerk-Monitor")
        tabs.addTab(GatewayTab(), "5. MAVLink-Gateway")
        tabs.addTab(RemoteTab(), "6. Fernverwaltung")
        self.setCentralWidget(tabs)


def main():
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
