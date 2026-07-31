#pragma once
#include <stddef.h>
#include <stdint.h>

#include "../mesh/packet.h"

// End-to-end payload encryption: AES-128-GCM with a network-wide PSK.
// Routers never hold the key — they forward ciphertext untouched.
//
//   Nonce (12 B) = src (2 B LE) | pkt_id (4 B LE) | 6 x 0x00
//   AAD          = first 10 header bytes (hop_limit excluded)
//   Tag          = GCM tag truncated to 4 B (airtime)
//
// pkt_id is strictly monotonic per node and bumped by 1024 on every boot,
// so a (src, pkt_id) nonce is never reused under one PSK.

class MeshCrypto {
 public:
  bool begin(const uint8_t psk[16]);
  bool ready() const { return ready_; }

  // out must hold len bytes, tag MESH_TAG_LEN bytes. Header must already
  // carry the final src/pkt_id/flags values.
  bool encrypt(const MeshHeader &h, const uint8_t *in, size_t len, uint8_t *out,
               uint8_t *tag);
  bool decrypt(const MeshHeader &h, const uint8_t *in, size_t len,
               const uint8_t *tag, uint8_t *out);

 private:
  void buildNonce(const MeshHeader &h, uint8_t nonce[12]);
  bool ready_ = false;
  void *ctx_ = nullptr;  // mbedtls_gcm_context, hidden from the header
};

extern MeshCrypto Crypto;
