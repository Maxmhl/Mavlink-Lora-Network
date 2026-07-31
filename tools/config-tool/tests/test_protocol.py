import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from loramesh_tool import protocol  # noqa: E402


def test_header_roundtrip():
    h = protocol.Header(flags=protocol.FLAG_ENCRYPTED, topic=0x01,
                        src=0x1234, dst=0xABCD, pkt_id=0xDEADBEEF, hop_limit=3)
    raw = h.pack()
    assert len(raw) == protocol.HEADER_LEN
    h2 = protocol.Header.unpack(raw)
    assert (h2.flags, h2.topic, h2.src, h2.dst, h2.pkt_id, h2.hop_limit) == (
        protocol.FLAG_ENCRYPTED, 0x01, 0x1234, 0xABCD, 0xDEADBEEF, 3)


def test_header_layout_matches_firmware():
    # Byte-exact fixture for firmware/src/mesh/packet.h (little-endian).
    h = protocol.Header(flags=0x01, topic=0x02, src=0x0102, dst=0xFFFF,
                        pkt_id=0x04030201, hop_limit=3)
    assert h.pack().hex() == "1102" + "0201" + "ffff" + "01020304" + "03"


def test_bad_version_rejected():
    raw = bytearray(protocol.Header().pack())
    raw[0] = 0x20  # version 2
    with pytest.raises(ValueError):
        protocol.Header.unpack(bytes(raw))


def test_topic_masks():
    mask = protocol.mask_from_topics(["mavlink", "weather"])
    assert mask == (1 << 1) | (1 << 2)
    assert set(protocol.topics_from_mask(mask)) == {"mavlink", "weather"}


@pytest.mark.parametrize("freq,dbm,ok", [
    (869.525, 27, True),   # 10 % band allows 500 mW ERP
    (869.525, 28, False),
    (868.1, 14, True),
    (868.1, 20, False),    # 1 % band capped at 25 mW ERP
    (868.65, 14, False),   # gap between sub-bands
    (870.5, 14, False),    # outside EU868
])
def test_eu868_validation(freq, dbm, ok):
    assert protocol.validate_phy(freq, 7, 125.0, 5, dbm)[0] == ok
