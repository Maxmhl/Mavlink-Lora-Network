"""Serial config-protocol client: reset the device, enter config mode via
handshake, exchange JSON lines."""

from __future__ import annotations

import json
import time

import serial

from .protocol import HANDSHAKE
from .ports import PortInfo

BAUD = 115200


class ConfigError(Exception):
    pass


class ConfigClient:
    def __init__(self, port: str, timeout: float = 1.0):
        self.port = port
        self.ser = serial.Serial(port, BAUD, timeout=timeout)

    # -- connection ----------------------------------------------------------

    def _pulse_reset(self):
        """Toggle EN via RTS (classic auto-reset wiring; on native USB-CDC the
        ROM reacts to the same DTR/RTS pattern)."""
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(0.1)
        self.ser.rts = False
        time.sleep(0.1)

    def enter_config_mode(self, reset: bool = True, window_s: float = 4.0) -> dict:
        """Reset the device and send the handshake during the boot window.
        Returns the device's handshake response. Works without reset too if
        the device role keeps the console always active (node/router)."""
        if reset:
            self.ser.reset_input_buffer()
            self._pulse_reset()

        deadline = time.monotonic() + window_s
        self.ser.timeout = 0.15
        while time.monotonic() < deadline:
            self.ser.write((HANDSHAKE + "\n").encode())
            self.ser.flush()
            line = self.ser.readline()
            while line:
                try:
                    obj = json.loads(line.decode(errors="replace").strip())
                except (ValueError, UnicodeDecodeError):
                    obj = None
                if obj and obj.get("mode") == "config":
                    self.ser.timeout = 2.0
                    return obj
                line = self.ser.readline()
        raise ConfigError(
            "Kein Konfigmodus-Handshake — Gerät geflasht und angeschlossen? "
            "Beim Gateway ggf. Reset-Taste drücken und sofort verbinden."
        )

    # -- commands ------------------------------------------------------------

    def command(self, cmd: dict, timeout: float = 3.0) -> dict:
        self.ser.reset_input_buffer()
        self.ser.write((json.dumps(cmd) + "\n").encode())
        self.ser.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.ser.readline().decode(errors="replace").strip()
            if not line or line.startswith("#"):
                continue
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if "evt" in obj:  # async event line, not our reply
                continue
            return obj
        raise ConfigError(f"Timeout bei Kommando {cmd.get('cmd')}")

    def get_config(self) -> dict:
        return self._checked({"cmd": "get"})

    def set_config(self, values: dict) -> dict:
        return self._checked({"cmd": "set", **values})

    def status(self) -> dict:
        return self._checked({"cmd": "status"})

    def reboot(self):
        try:
            self.command({"cmd": "reboot"}, timeout=1.0)
        except ConfigError:
            pass  # device resets before replying sometimes

    def exit_config(self):
        try:
            self.command({"cmd": "exit"}, timeout=1.0)
        except ConfigError:
            pass

    def _checked(self, cmd: dict) -> dict:
        resp = self.command(cmd)
        if not resp.get("ok", False):
            raise ConfigError(resp.get("err", "Gerät meldet Fehler"))
        return resp

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def probe_port(info: PortInfo, reset: bool = True) -> PortInfo:
    """Identify the device on a port: role, node id, variant. Leaves a
    gateway back in passthrough mode afterwards."""
    with ConfigClient(info.device) as c:
        hello = c.enter_config_mode(reset=reset)
        cfg = c.get_config()
        info.role = cfg.get("role")
        info.node_id = cfg.get("node_id")
        info.variant = cfg.get("variant") or hello.get("variant")
        info.extra = cfg
        c.exit_config()
    return info
