"""COM port enumeration with ESP32-typical USB bridge detection."""

from __future__ import annotations

from dataclasses import dataclass, field

from serial.tools import list_ports

# (VID, PID) -> chip label
KNOWN_BRIDGES = {
    (0x10C4, 0xEA60): "CP210x",
    (0x1A86, 0x7523): "CH340",
    (0x1A86, 0x55D4): "CH9102",
    (0x303A, 0x1001): "ESP32-S3 USB",
    (0x303A, 0x0002): "ESP32-S3 CDC",
}


@dataclass
class PortInfo:
    device: str
    description: str = ""
    bridge: str = ""
    # filled by probing (config_client.probe_port)
    role: str | None = None
    node_id: int | None = None
    variant: str | None = None
    extra: dict = field(default_factory=dict)

    @property
    def label(self) -> str:
        parts = [self.device]
        if self.bridge:
            parts.append(self.bridge)
        if self.description and self.description != "n/a":
            parts.append(self.description)
        return " — ".join(parts)


def scan_ports() -> list[PortInfo]:
    result = []
    for p in sorted(list_ports.comports(), key=lambda x: x.device):
        bridge = KNOWN_BRIDGES.get((p.vid or 0, p.pid or 0), "")
        result.append(PortInfo(p.device, p.description or "", bridge))
    return result
