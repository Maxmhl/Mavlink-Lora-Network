"""Serial config-protocol client: reset the device, enter config mode via
handshake, exchange JSON lines."""

from __future__ import annotations

import json
import time

import serial

from .protocol import BROADCAST, HANDSHAKE
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

    # -- remote management (via this device as radio bridge) -----------------

    def _read_admin_events(self, deadline: float, on_event) -> None:
        """Read serial lines until deadline, forwarding {"evt":"admin"} lines
        (src, data) to on_event. on_event returning True stops early."""
        while time.monotonic() < deadline:
            raw = self.ser.readline().decode(errors="replace").strip()
            if not raw or raw.startswith("#"):
                continue
            try:
                obj = json.loads(raw)
            except ValueError:
                continue
            if obj.get("evt") != "admin":
                continue
            if on_event(obj.get("src"), obj.get("data") or {}):
                return

    def remote(self, dst: int, data: dict, timeout: float = 10.0,
               retries: int = 3) -> dict:
        """Send one management command to node `dst` and wait for its reply.
        Retries on timeout (LoRa is lossy; commands are idempotent)."""
        last_err = None
        for _ in range(max(1, retries)):
            try:
                self._checked({"cmd": "remote", "dst": dst, "data": data})
            except ConfigError as e:
                raise ConfigError(f"Brücken-Node: {e}") from e
            result: list[dict] = []

            def on_event(src, payload):
                if src == dst:
                    result.append(payload)
                    return True
                return False

            self._read_admin_events(time.monotonic() + timeout, on_event)
            if result:
                return result[0]
            last_err = ConfigError(
                f"Keine Antwort von Node 0x{dst:04X} (Timeout)")
        raise last_err

    def discover(self, duration_s: float = 8.0) -> dict[int, dict]:
        """Broadcast ping; collect every device that answers within the
        window. Returns {node_id: ping_response}."""
        self._checked({"cmd": "remote", "dst": BROADCAST,
                       "data": {"acmd": "ping"}})
        found: dict[int, dict] = {}

        def on_event(src, payload):
            if src is not None:
                found[src] = payload
            return False

        self._read_admin_events(time.monotonic() + duration_s, on_event)
        return found

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
