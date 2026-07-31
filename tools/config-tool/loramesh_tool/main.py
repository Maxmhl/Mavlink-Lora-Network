"""Application entry point."""

from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget

from . import __version__
from .config_tab import ConfigTab
from .flash_tab import FlashTab
from .gateway_tab import GatewayTab
from .monitor_tab import MonitorTab


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(f"LoRa-Mesh Konfigurator v{__version__}")
        self.resize(860, 720)

        tabs = QTabWidget()
        tabs.addTab(FlashTab(), "1. Flashen")
        tabs.addTab(ConfigTab(), "2. Konfiguration")
        tabs.addTab(MonitorTab(), "3. Monitor")
        tabs.addTab(GatewayTab(), "4. MAVLink-Gateway")
        self.setCentralWidget(tabs)


def main():
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
