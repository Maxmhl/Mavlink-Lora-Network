#include "fragment.h"

#include <Arduino.h>
#include <string.h>

bool Fragmenter::send(uint8_t topic, uint16_t dst, const uint8_t *data,
                      size_t len) {
  if (len == 0 || len > MESH_MAX_MESSAGE) return false;

  if (len <= MESH_MAX_PAYLOAD) {
    return Mesh.sendPacket(topic, dst, data, len);
  }

  uint8_t total = (len + MESH_MAX_FRAG_DATA - 1) / MESH_MAX_FRAG_DATA;
  if (total > 15) return false;
  uint8_t seq = txSeq_++;

  bool ok = true;
  for (uint8_t idx = 0; idx < total; idx++) {
    size_t off = (size_t)idx * MESH_MAX_FRAG_DATA;
    size_t chunk = min(len - off, MESH_MAX_FRAG_DATA);
    uint8_t buf[MESH_MAX_PAYLOAD];
    buf[0] = seq;
    buf[1] = (idx << 4) | (total & 0x0F);
    memcpy(buf + MESH_FRAG_HDR_LEN, data + off, chunk);
    ok &= Mesh.sendPacket(topic, dst, buf, chunk + MESH_FRAG_HDR_LEN,
                          MESH_FLAG_FRAGMENT);
  }
  return ok;
}

const uint8_t *Fragmenter::feed(const MeshHeader &h, const uint8_t *payload,
                                size_t len, size_t *outLen) {
  *outLen = 0;
  if (!h.fragment()) {
    // Not fragmented — hand straight back.
    if (len > MESH_MAX_MESSAGE) return nullptr;
    memcpy(assembled_, payload, len);
    *outLen = len;
    return assembled_;
  }
  if (len <= MESH_FRAG_HDR_LEN) return nullptr;

  uint8_t seq = payload[0];
  uint8_t idx = payload[1] >> 4;
  uint8_t total = payload[1] & 0x0F;
  if (total == 0 || idx >= total) return nullptr;
  size_t chunk = len - MESH_FRAG_HDR_LEN;

  uint32_t now = millis();
  Slot *slot = nullptr;
  for (auto &s : slots_) {
    if (s.used && now - s.started_ms > TIMEOUT_MS) s.used = false;
    if (s.used && s.src == h.src && s.msg_seq == seq && s.total == total) {
      slot = &s;
      break;
    }
  }
  if (!slot) {
    for (auto &s : slots_) {
      if (!s.used) {
        slot = &s;
        break;
      }
    }
    if (!slot) slot = &slots_[0];  // steal the first slot
    slot->used = true;
    slot->src = h.src;
    slot->msg_seq = seq;
    slot->total = total;
    slot->received_mask = 0;
    slot->started_ms = now;
  }

  memcpy(slot->data[idx], payload + MESH_FRAG_HDR_LEN, chunk);
  slot->frag_len[idx] = chunk;
  slot->received_mask |= (1U << idx);

  uint16_t complete = (1U << total) - 1;
  if ((slot->received_mask & complete) != complete) return nullptr;

  size_t pos = 0;
  for (uint8_t i = 0; i < total; i++) {
    memcpy(assembled_ + pos, slot->data[i], slot->frag_len[i]);
    pos += slot->frag_len[i];
  }
  slot->used = false;
  *outLen = pos;
  return assembled_;
}
