#pragma once
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// On-air packet format (v1). Multi-byte fields are little-endian.
//
//   Header (11 B, cleartext — routers must read it to forward):
//     [0]    ver_flags  bits 7..4 = protocol version (1)
//                       bit 0 = encrypted, bit 1 = fragment, bit 2 = want_ack
//     [1]    topic      see Topic below
//     [2:3]  src        node id (uint16 LE)
//     [4:5]  dst        node id, 0xFFFF = broadcast
//     [6:9]  pkt_id     uint32 LE, monotonic per node (nonce component!)
//     [10]   hop_limit  bits 3..0 remaining hops (decremented by relays)
//
//   Payload: encrypted with AES-128-GCM.
//     Nonce (12 B) = src (2 B LE) | pkt_id (4 B LE) | 6 x 0x00
//     AAD          = header bytes [0..9] (hop_limit excluded — it mutates
//                    en route)
//     Tag          = first 4 B of the GCM tag, appended after the ciphertext
//
//   Fragmented messages (flag bit 1): payload starts with a 2 B frag header
//     [0] msg_seq   — same for all fragments of one message
//     [1] frag_idx << 4 | frag_total   (1-based total, 0-based idx, max 15)
//   Every fragment is an independent packet with its own pkt_id (own nonce)
//   and is encrypted separately.
// ---------------------------------------------------------------------------

static constexpr uint8_t MESH_PROTO_VERSION = 1;
static constexpr size_t MESH_HEADER_LEN = 11;
static constexpr size_t MESH_AAD_LEN = 10;  // header without hop_limit
static constexpr size_t MESH_TAG_LEN = 4;
static constexpr size_t MESH_MAX_FRAME = 255;
static constexpr size_t MESH_MAX_PAYLOAD =
    MESH_MAX_FRAME - MESH_HEADER_LEN - MESH_TAG_LEN;                  // 240
static constexpr size_t MESH_FRAG_HDR_LEN = 2;
static constexpr size_t MESH_MAX_FRAG_DATA = MESH_MAX_PAYLOAD - MESH_FRAG_HDR_LEN;  // 238
static constexpr size_t MESH_MAX_MESSAGE = 15 * MESH_MAX_FRAG_DATA;

static constexpr uint16_t MESH_BROADCAST = 0xFFFF;

enum MeshFlags : uint8_t {
  MESH_FLAG_ENCRYPTED = 0x01,
  MESH_FLAG_FRAGMENT = 0x02,
  MESH_FLAG_WANT_ACK = 0x04,
};

// Topic ids double as bit positions in the rx/tx subscription masks
// (all topic ids must stay < 32).
enum Topic : uint8_t {
  TOPIC_MAVLINK = 0x01,
  TOPIC_WEATHER = 0x02,
  TOPIC_POSITION = 0x03,
  TOPIC_STATUS = 0x04,
  TOPIC_GENERIC_BASE = 0x10,  // 0x10..0x1F free for user-defined payloads
};

static constexpr uint32_t topicBit(uint8_t topic) { return 1UL << (topic & 31); }

struct MeshHeader {
  uint8_t ver_flags = MESH_PROTO_VERSION << 4;
  uint8_t topic = 0;
  uint16_t src = 0;
  uint16_t dst = MESH_BROADCAST;
  uint32_t pkt_id = 0;
  uint8_t hop_limit = 0;

  uint8_t version() const { return ver_flags >> 4; }
  bool encrypted() const { return ver_flags & MESH_FLAG_ENCRYPTED; }
  bool fragment() const { return ver_flags & MESH_FLAG_FRAGMENT; }
};

inline size_t meshPackHeader(const MeshHeader &h, uint8_t *buf) {
  buf[0] = h.ver_flags;
  buf[1] = h.topic;
  buf[2] = h.src & 0xFF;
  buf[3] = h.src >> 8;
  buf[4] = h.dst & 0xFF;
  buf[5] = h.dst >> 8;
  buf[6] = h.pkt_id & 0xFF;
  buf[7] = (h.pkt_id >> 8) & 0xFF;
  buf[8] = (h.pkt_id >> 16) & 0xFF;
  buf[9] = (h.pkt_id >> 24) & 0xFF;
  buf[10] = h.hop_limit & 0x0F;
  return MESH_HEADER_LEN;
}

inline bool meshUnpackHeader(const uint8_t *buf, size_t len, MeshHeader &h) {
  if (len < MESH_HEADER_LEN) return false;
  h.ver_flags = buf[0];
  h.topic = buf[1];
  h.src = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
  h.dst = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
  h.pkt_id = (uint32_t)buf[6] | ((uint32_t)buf[7] << 8) |
             ((uint32_t)buf[8] << 16) | ((uint32_t)buf[9] << 24);
  h.hop_limit = buf[10] & 0x0F;
  return h.version() == MESH_PROTO_VERSION;
}
