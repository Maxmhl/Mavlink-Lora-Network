#pragma once
#include <stddef.h>
#include <stdint.h>

#include "packet.h"
#include "router.h"

// Fragmentation for messages larger than one LoRa frame (MAVLink v2 frames
// can reach 280 B). Each fragment is an independent mesh packet (own pkt_id,
// own GCM nonce) carrying a 2 B fragment header inside the encrypted payload:
//   [0] msg_seq, [1] frag_idx << 4 | frag_total
class Fragmenter {
 public:
  // Splits data and sends via Mesh. Messages that fit a single frame are
  // sent without the fragment header/flag.
  bool send(uint8_t topic, uint16_t dst, const uint8_t *data, size_t len);

  // Feed a received fragment payload (already decrypted). Returns a pointer
  // to the reassembled message (valid until the next call) and sets outLen,
  // or nullptr while incomplete.
  const uint8_t *feed(const MeshHeader &h, const uint8_t *payload, size_t len,
                      size_t *outLen);

 private:
  struct Slot {
    bool used = false;
    uint16_t src = 0;
    uint8_t msg_seq = 0;
    uint8_t total = 0;
    uint16_t received_mask = 0;
    size_t frag_len[15] = {0};
    uint8_t data[15][MESH_MAX_FRAG_DATA];
    uint32_t started_ms = 0;
  };
  static constexpr size_t SLOTS = 3;
  static constexpr uint32_t TIMEOUT_MS = 5000;

  Slot slots_[SLOTS];
  uint8_t txSeq_ = 0;
  uint8_t assembled_[MESH_MAX_MESSAGE];
};
