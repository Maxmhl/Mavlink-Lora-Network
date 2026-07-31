"""Shared protocol definitions — must stay in sync with firmware/src/mesh/packet.h
and firmware/src/crypto/crypto.cpp."""

from __future__ import annotations

import secrets
import struct
from dataclasses import dataclass

PROTO_VERSION = 1
HEADER_LEN = 11
AAD_LEN = 10  # header without hop_limit
TAG_LEN = 4
BROADCAST = 0xFFFF

FLAG_ENCRYPTED = 0x01
FLAG_FRAGMENT = 0x02

HANDSHAKE = "+++CLMESH-CFG+++"

ROLES = ["node", "router", "mavlink_gateway"]

TOPICS = {
    "mavlink": 0x01,
    "weather": 0x02,
    "position": 0x03,
    "status": 0x04,
    "admin": 0x05,
    "generic": 0x10,
}

# Remote-management commands (TOPIC_ADMIN payload, {"acmd": ...}).
# set/reboot/factory require unicast; keys are never settable remotely.
ADMIN_COMMANDS = ("ping", "status", "get", "set", "reboot", "factory")


def topic_bit(topic_id: int) -> int:
    return 1 << (topic_id & 31)


def mask_from_topics(names: list[str]) -> int:
    mask = 0
    for n in names:
        mask |= topic_bit(TOPICS[n])
    return mask


def topics_from_mask(mask: int) -> list[str]:
    return [n for n, t in TOPICS.items() if mask & topic_bit(t)]


# --- EU868 sub-bands (same table as firmware/src/radio/duty_cycle.cpp) ------

@dataclass
class Band:
    lo: float
    hi: float
    duty: float
    max_dbm: int


EU868_BANDS = [
    Band(863.0, 868.0, 0.01, 14),
    Band(868.0, 868.6, 0.01, 14),
    Band(868.7, 869.2, 0.001, 14),
    Band(869.4, 869.65, 0.10, 27),
    Band(869.7, 870.0, 0.01, 14),
]


def find_band(freq_mhz: float) -> Band | None:
    for b in EU868_BANDS:
        if b.lo <= freq_mhz <= b.hi:
            return b
    return None


def validate_phy(freq: float, sf: int, bw: float, cr: int, tx_dbm: int):
    """Returns (ok, message). tx power above the band ERP limit is an error."""
    band = find_band(freq)
    if band is None:
        return False, f"{freq:.3f} MHz liegt außerhalb der EU868-Subbänder"
    if not 5 <= sf <= 12:
        return False, "SF muss zwischen 5 und 12 liegen"
    if bw not in (125.0, 250.0, 500.0):
        return False, "Bandbreite muss 125/250/500 kHz sein"
    if not 5 <= cr <= 8:
        return False, "Coding Rate muss 4/5..4/8 (5..8) sein"
    if tx_dbm > band.max_dbm:
        return False, (
            f"Max. {band.max_dbm} dBm ERP in diesem Subband "
            f"(Duty-Cycle {band.duty * 100:g} %)"
        )
    return True, f"OK — Subband {band.lo}-{band.hi} MHz, Duty-Cycle {band.duty * 100:g} %"


# --- packet header -----------------------------------------------------------

_HDR = struct.Struct("<BBHHIB")


@dataclass
class Header:
    flags: int = 0
    topic: int = 0
    src: int = 0
    dst: int = BROADCAST
    pkt_id: int = 0
    hop_limit: int = 3

    def pack(self) -> bytes:
        ver_flags = (PROTO_VERSION << 4) | (self.flags & 0x0F)
        return _HDR.pack(ver_flags, self.topic, self.src, self.dst,
                         self.pkt_id, self.hop_limit & 0x0F)

    @classmethod
    def unpack(cls, data: bytes) -> "Header":
        ver_flags, topic, src, dst, pkt_id, hops = _HDR.unpack(data[:HEADER_LEN])
        if ver_flags >> 4 != PROTO_VERSION:
            raise ValueError("unknown protocol version")
        return cls(ver_flags & 0x0F, topic, src, dst, pkt_id, hops & 0x0F)


# --- crypto (mirror of the firmware AES-128-GCM scheme) ---------------------

def _nonce(header: Header) -> bytes:
    return struct.pack("<HI", header.src, header.pkt_id) + b"\x00" * 6


def encrypt_payload(psk: bytes, header: Header, plaintext: bytes) -> bytes:
    """Returns ciphertext || 4-byte truncated GCM tag."""
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    header.flags |= FLAG_ENCRYPTED
    enc = Cipher(algorithms.AES(psk), modes.GCM(_nonce(header))).encryptor()
    enc.authenticate_additional_data(header.pack()[:AAD_LEN])
    ct = enc.update(plaintext) + enc.finalize()
    return ct + enc.tag[:TAG_LEN]


def decrypt_payload(psk: bytes, header: Header, body: bytes) -> bytes:
    """body = ciphertext || 4-byte tag. Raises InvalidTag on failure."""
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    ct, tag = body[:-TAG_LEN], body[-TAG_LEN:]
    dec = Cipher(
        algorithms.AES(psk),
        modes.GCM(_nonce(header), tag=tag, min_tag_length=TAG_LEN),
    ).decryptor()
    dec.authenticate_additional_data(header.pack()[:AAD_LEN])
    return dec.update(ct) + dec.finalize()


def generate_psk() -> str:
    """New random 128-bit network key as 32 hex chars."""
    return secrets.token_hex(16)
