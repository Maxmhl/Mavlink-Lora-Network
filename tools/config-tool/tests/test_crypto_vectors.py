"""End-to-end crypto vectors mirroring firmware/src/crypto/crypto.cpp:
AES-128-GCM, nonce = src|pkt_id|000000, AAD = header[0:10], 4-byte tag."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from loramesh_tool import protocol  # noqa: E402

PSK = bytes.fromhex("000102030405060708090a0b0c0d0e0f")


def make_header(pkt_id=42):
    return protocol.Header(flags=0, topic=protocol.TOPICS["weather"],
                           src=0x0011, dst=protocol.BROADCAST,
                           pkt_id=pkt_id, hop_limit=3)


def test_roundtrip():
    h = make_header()
    plaintext = b"hello mesh"
    body = protocol.encrypt_payload(PSK, h, plaintext)
    assert len(body) == len(plaintext) + protocol.TAG_LEN
    assert protocol.decrypt_payload(PSK, h, body) == plaintext


def test_tamper_detection():
    from cryptography.exceptions import InvalidTag

    h = make_header()
    body = bytearray(protocol.encrypt_payload(PSK, h, b"hello mesh"))
    body[0] ^= 0x01
    with pytest.raises(InvalidTag):
        protocol.decrypt_payload(PSK, h, bytes(body))


def test_header_is_authenticated():
    """Flipping dst (part of the AAD) must invalidate the tag."""
    from cryptography.exceptions import InvalidTag

    h = make_header()
    body = protocol.encrypt_payload(PSK, h, b"hello mesh")
    h.dst = 0x0001
    with pytest.raises(InvalidTag):
        protocol.decrypt_payload(PSK, h, body)


def test_hop_limit_not_authenticated():
    """Relays decrement hop_limit — decryption must still succeed."""
    h = make_header()
    body = protocol.encrypt_payload(PSK, h, b"hello mesh")
    h.hop_limit = 0
    assert protocol.decrypt_payload(PSK, h, body) == b"hello mesh"


def test_nonce_uniqueness_per_pkt_id():
    h1, h2 = make_header(1), make_header(2)
    c1 = protocol.encrypt_payload(PSK, h1, b"same plaintext")
    c2 = protocol.encrypt_payload(PSK, h2, b"same plaintext")
    assert c1 != c2


def test_known_vector():
    """Pinned vector — documents the exact wire format. If this breaks, the
    firmware and tool are no longer interoperable."""
    h = protocol.Header(flags=0, topic=0x02, src=0x0011, dst=0xFFFF,
                        pkt_id=42, hop_limit=3)
    body = protocol.encrypt_payload(PSK, h, b"hello mesh")
    assert h.flags & protocol.FLAG_ENCRYPTED
    assert body.hex() == EXPECTED_VECTOR


# Computed with cryptography's AES-128-GCM using the scheme above
# (see docs/protokoll.md).
EXPECTED_VECTOR = "69ee6643de79a4edc339b4d0f1a2"
