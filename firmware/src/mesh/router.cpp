#include "router.h"

#include <string.h>

#include "../crypto/crypto.h"

MeshRouter Mesh;

void MeshRouter::begin(const NodeConfig *cfg) { cfg_ = cfg; }

bool MeshRouter::isDuplicate(uint16_t src, uint32_t pkt_id) {
  for (size_t i = 0; i < SEEN_SLOTS; i++) {
    if (seen_[i].src == src && seen_[i].pkt_id == pkt_id) return true;
  }
  seen_[seenHead_] = {src, pkt_id};
  seenHead_ = (seenHead_ + 1) % SEEN_SLOTS;
  return false;
}

// Sliding-window replay protection (per source, window of 32 pkt_ids).
// Returns true if the packet id is fresh.
bool MeshRouter::replayCheck(uint16_t src, uint32_t pkt_id) {
  ReplayEntry *slot = nullptr;
  ReplayEntry *oldest = &replay_[0];
  for (auto &e : replay_) {
    if (e.used && e.src == src) {
      slot = &e;
      break;
    }
    if (!e.used) {
      oldest = &e;
    }
  }
  if (!slot) {
    // Recycle a free (or arbitrary) slot for this source.
    slot = oldest->used ? &replay_[0] : oldest;
    slot->used = true;
    slot->src = src;
    slot->max_id = pkt_id;
    slot->window = 0;
    return true;
  }
  if (pkt_id > slot->max_id) {
    uint32_t shift = pkt_id - slot->max_id;
    slot->window = (shift >= 32) ? 0 : (slot->window << shift) | (1UL << (shift - 1));
    slot->max_id = pkt_id;
    return true;
  }
  uint32_t age = slot->max_id - pkt_id;
  if (age == 0 || age > 32) return false;  // current or too old -> reject
  uint32_t bit = 1UL << (age - 1);
  if (slot->window & bit) return false;  // already seen
  slot->window |= bit;
  return true;
}

void MeshRouter::noteNeighbor(uint16_t src, float rssi, float snr) {
  Neighbor *slot = nullptr;
  uint32_t oldestAge = 0;
  for (auto &n : neighbors_) {
    if (n.id == src) {
      slot = &n;
      break;
    }
    uint32_t age = millis() - n.last_ms;
    if (n.id == 0 || age >= oldestAge) {
      oldestAge = (n.id == 0) ? UINT32_MAX : age;
      slot = &n;
    }
  }
  if (slot->id != src) {
    slot->id = src;
    slot->packets = 0;
  }
  slot->rssi = rssi;
  slot->snr = snr;
  slot->last_ms = millis();
  slot->packets++;
}

bool MeshRouter::enqueue(const uint8_t *frame, size_t len, uint32_t delay_ms) {
  for (auto &q : queue_) {
    if (!q.used) {
      memcpy(q.frame, frame, len);
      q.len = len;
      q.due_ms = millis() + delay_ms;
      q.tries = 0;
      q.used = true;
      return true;
    }
  }
  return false;  // queue full, drop
}

void MeshRouter::drainQueue() {
  uint32_t now = millis();
  for (auto &q : queue_) {
    if (!q.used || (int32_t)(now - q.due_ms) < 0) continue;
    if (LoRaRadio.send(q.frame, q.len)) {
      q.used = false;
    } else if (++q.tries >= 5) {
      q.used = false;  // duty cycle exhausted or channel jammed — drop
    } else {
      q.due_ms = now + random(30, 120);
    }
  }
}

bool MeshRouter::sendPacket(uint8_t topic, uint16_t dst, const uint8_t *payload,
                            size_t len, uint8_t flags) {
  if (!cfg_ || len == 0 || len > MESH_MAX_PAYLOAD) return false;

  MeshHeader h;
  h.topic = topic;
  h.src = cfg_->node_id;
  h.dst = dst;
  h.pkt_id = Config.nextPacketId();
  h.hop_limit = cfg_->hop_limit;
  h.ver_flags = (MESH_PROTO_VERSION << 4) | (flags & MESH_FLAG_FRAGMENT);

  uint8_t frame[MESH_MAX_FRAME];
  size_t pos = 0;

  if (Crypto.ready()) {
    h.ver_flags |= MESH_FLAG_ENCRYPTED;
    pos = meshPackHeader(h, frame);
    uint8_t tag[MESH_TAG_LEN];
    if (!Crypto.encrypt(h, payload, len, frame + pos, tag)) return false;
    pos += len;
    memcpy(frame + pos, tag, MESH_TAG_LEN);
    pos += MESH_TAG_LEN;
  } else {
    // Unencrypted operation is only intended for bring-up/testing.
    pos = meshPackHeader(h, frame);
    memcpy(frame + pos, payload, len);
    pos += len;
  }

  // Mark own packets as seen so a relayed echo is not re-flooded/delivered.
  isDuplicate(h.src, h.pkt_id);
  return enqueue(frame, pos, 0);
}

void MeshRouter::handleFrame(const uint8_t *frame, size_t len, float rssi,
                             float snr) {
  MeshHeader h;
  if (!meshUnpackHeader(frame, len, h)) return;
  if (h.src == cfg_->node_id) return;  // echo of our own packet

  noteNeighbor(h.src, rssi, snr);

  if (isDuplicate(h.src, h.pkt_id)) {
    dupCount++;
    return;
  }

  bool isRouter = cfg_->role == NodeRole::ROUTER;
  bool relayEnabled = isRouter || cfg_->relay;
  bool forMe = (h.dst == cfg_->node_id);
  bool broadcast = (h.dst == MESH_BROADCAST);

  // ---- forward (store & forward flooding) ----
  // Unicast packets addressed to us terminate here; everything else is
  // re-flooded by relays while hops remain.
  if (relayEnabled && !forMe && h.hop_limit > 0) {
    uint8_t fwd[MESH_MAX_FRAME];
    memcpy(fwd, frame, len);
    fwd[10] = (h.hop_limit - 1) & 0x0F;
    // SNR-weighted jitter: the worse we hear it, the sooner we repeat.
    float x = snr + 20.0f;  // typical SNR range about -20..+10 dB
    if (x < 0) x = 0;
    uint32_t delay_ms = (uint32_t)(x * 8.0f) + random(20, 80);
    if (enqueue(fwd, len, delay_ms)) forwardedCount++;
  }

  // ---- deliver to local applications ----
  if (cfg_->role == NodeRole::ROUTER) return;  // routers never consume
  if (!forMe && !(broadcast && (cfg_->rx_topics & topicBit(h.topic)))) return;
  if (!replayCheck(h.src, h.pkt_id)) return;

  const uint8_t *body = frame + MESH_HEADER_LEN;
  size_t bodyLen = len - MESH_HEADER_LEN;
  uint8_t plain[MESH_MAX_PAYLOAD];

  if (h.encrypted()) {
    if (bodyLen <= MESH_TAG_LEN || !Crypto.ready()) return;
    size_t ctLen = bodyLen - MESH_TAG_LEN;
    if (!Crypto.decrypt(h, body, ctLen, body + ctLen, plain)) {
      authFailCount++;
      return;
    }
    deliveredCount++;
    if (deliver_) deliver_(h, plain, ctLen);
  } else {
    deliveredCount++;
    if (deliver_) deliver_(h, body, bodyLen);
  }
}

void MeshRouter::loop() {
  uint8_t buf[MESH_MAX_FRAME];
  float rssi = 0, snr = 0;
  size_t len;
  while ((len = LoRaRadio.receive(buf, sizeof(buf), &rssi, &snr)) > 0) {
    handleFrame(buf, len, rssi, snr);
  }
  drainQueue();
}
