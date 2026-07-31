"""Remote-management (TOPIC_ADMIN) protocol tests: separate admin key,
key isolation, and command constants."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from loramesh_tool import protocol  # noqa: E402

DATA_PSK = bytes.fromhex("000102030405060708090a0b0c0d0e0f")
ADMIN_PSK = bytes.fromhex("ffeeddccbbaa99887766554433221100")


def make_admin_header(pkt_id=7, dst=0x0022):
    return protocol.Header(flags=0, topic=protocol.TOPICS["admin"],
                           src=0x0011, dst=dst, pkt_id=pkt_id, hop_limit=3)


def test_admin_topic_id_and_mask():
    assert protocol.TOPICS["admin"] == 0x05
    assert protocol.topic_bit(protocol.TOPICS["admin"]) == 1 << 5
    assert "admin" in protocol.topics_from_mask(1 << 5)


def test_admin_roundtrip_with_admin_key():
    h = make_admin_header()
    payload = b'{"acmd":"ping"}'
    body = protocol.encrypt_payload(ADMIN_PSK, h, payload)
    assert protocol.decrypt_payload(ADMIN_PSK, h, body) == payload


def test_key_isolation_between_data_and_admin():
    """A router holding only the admin key must not be able to decrypt data
    traffic — and vice versa."""
    from cryptography.exceptions import InvalidTag

    h = make_admin_header()
    body = protocol.encrypt_payload(ADMIN_PSK, h, b'{"acmd":"reboot"}')
    with pytest.raises(InvalidTag):
        protocol.decrypt_payload(DATA_PSK, h, body)

    h2 = protocol.Header(flags=0, topic=protocol.TOPICS["weather"],
                         src=0x0011, dst=protocol.BROADCAST,
                         pkt_id=8, hop_limit=3)
    data_body = protocol.encrypt_payload(DATA_PSK, h2, b"weather-data")
    with pytest.raises(InvalidTag):
        protocol.decrypt_payload(ADMIN_PSK, h2, data_body)


def test_admin_command_names():
    assert set(protocol.ADMIN_COMMANDS) == {
        "ping", "status", "get", "set", "reboot", "factory"}
