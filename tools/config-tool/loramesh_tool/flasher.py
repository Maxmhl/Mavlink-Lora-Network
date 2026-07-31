"""esptool integration: flash the merged firmware image in a Qt worker thread."""

from __future__ import annotations

import contextlib
import io

from PySide6.QtCore import QThread, Signal

BOARD_PROFILES = {
    "Heltec V3 (ESP32-S3)": {"chip": "esp32s3", "env": "heltec_v3"},
    "LilyGo T-Beam 1W (ESP32-S3)": {"chip": "esp32s3", "env": "tbeam_1w"},
    "LilyGo T-Beam v1.1/v1.2 (ESP32)": {"chip": "esp32", "env": "tbeam_v12"},
    "SenseCAP Solar Node P1 (XIAO ESP32-S3)": {"chip": "esp32s3", "env": "sensecap_p1"},
}

# The PlatformIO post-build script produces firmware-merged.bin
# (bootloader + partitions + app) which is flashed in one piece at 0x0.
MERGED_OFFSET = "0x0"


class _SignalWriter(io.TextIOBase):
    def __init__(self, signal):
        self._signal = signal
        self._buf = ""

    def write(self, text):
        self._buf += text
        while "\n" in self._buf or "\r" in self._buf:
            for sep in ("\n", "\r"):
                if sep in self._buf:
                    line, self._buf = self._buf.split(sep, 1)
                    if line.strip():
                        self._signal.emit(line)
                    break
        return len(text)


class FlashWorker(QThread):
    """Runs esptool write_flash without blocking the UI."""

    progress = Signal(str)
    finished_ok = Signal()
    failed = Signal(str)

    def __init__(self, port: str, chip: str, firmware_path: str,
                 baud: int = 460800, erase: bool = False):
        super().__init__()
        self.port = port
        self.chip = chip
        self.firmware_path = firmware_path
        self.baud = baud
        self.erase = erase

    def run(self):
        import esptool

        args = [
            "--chip", self.chip,
            "--port", self.port,
            "--baud", str(self.baud),
            "--before", "default_reset",
            "--after", "hard_reset",
        ]
        if self.erase:
            args += ["erase_flash"]
        else:
            args += ["write_flash", "-z", MERGED_OFFSET, self.firmware_path]

        writer = _SignalWriter(self.progress)
        try:
            with contextlib.redirect_stdout(writer), contextlib.redirect_stderr(writer):
                esptool.main(args)
            self.finished_ok.emit()
        except SystemExit as e:
            if e.code in (0, None):
                self.finished_ok.emit()
            else:
                self.failed.emit(f"esptool beendet mit Code {e.code}")
        except Exception as e:  # noqa: BLE001 — surface everything in the UI
            self.failed.emit(str(e))
