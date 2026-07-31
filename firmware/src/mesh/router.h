#pragma once
#include <functional>

#include "../config/config_store.h"
#include "../radio/radio.h"
#include "packet.h"

// Managed flooding mesh:
//  - dedup via (src, pkt_id) LRU
//  - hop_limit decremented on relay, packet dropped at 0
//  - relays delay rebroadcast with SNR-weighted jitter (strong signal waits
//    longer, distant relays repeat first — extends coverage per hop)
//  - routers forward everything and never decrypt; nodes decrypt packets
//    addressed to them (dst == self or broadcast + subscribed topic)
//  - replay protection per source: sliding 32-packet window on pkt_id

struct Neighbor {
  uint16_t id = 0;
  float rssi = 0;
  float snr = 0;
  uint32_t last_ms = 0;
  uint32_t packets = 0;
};

class MeshRouter {
 public:
  static constexpr size_t NEIGHBOR_SLOTS = 16;

  using DeliverFn = std::function<void(const MeshHeader &, const uint8_t *data,
                                       size_t len)>;

  void begin(const NodeConfig *cfg);
  void loop();

  // Encrypts (when a PSK is set) and queues one mesh packet. payload must be
  // <= MESH_MAX_PAYLOAD. flags: MESH_FLAG_FRAGMENT for fragment packets.
  bool sendPacket(uint8_t topic, uint16_t dst, const uint8_t *payload,
                  size_t len, uint8_t flags = 0);

  void onDeliver(DeliverFn fn) { deliver_ = fn; }

  const Neighbor *neighbors() const { return neighbors_; }
  uint32_t deliveredCount = 0;
  uint32_t forwardedCount = 0;
  uint32_t dupCount = 0;
  uint32_t authFailCount = 0;

 private:
  struct QueueEntry {
    uint8_t frame[MESH_MAX_FRAME];
    size_t len = 0;
    uint32_t due_ms = 0;
    uint8_t tries = 0;
    bool used = false;
  };
  struct SeenEntry {
    uint16_t src = 0;
    uint32_t pkt_id = 0;
  };
  struct ReplayEntry {
    uint16_t src = 0;
    uint32_t max_id = 0;
    uint32_t window = 0;  // bitmap of max_id-1 .. max_id-32
    bool used = false;
  };

  void handleFrame(const uint8_t *frame, size_t len, float rssi, float snr);
  bool isDuplicate(uint16_t src, uint32_t pkt_id);
  bool replayCheck(uint16_t src, uint32_t pkt_id);
  void noteNeighbor(uint16_t src, float rssi, float snr);
  bool enqueue(const uint8_t *frame, size_t len, uint32_t delay_ms);
  void drainQueue();

  const NodeConfig *cfg_ = nullptr;
  DeliverFn deliver_;

  static constexpr size_t SEEN_SLOTS = 64;
  SeenEntry seen_[SEEN_SLOTS];
  size_t seenHead_ = 0;

  static constexpr size_t REPLAY_SLOTS = 16;
  ReplayEntry replay_[REPLAY_SLOTS];

  static constexpr size_t QUEUE_SLOTS = 8;
  QueueEntry queue_[QUEUE_SLOTS];

  Neighbor neighbors_[NEIGHBOR_SLOTS];
};

extern MeshRouter Mesh;
