#include "crypto.h"

#include <string.h>

MeshCrypto Crypto;
MeshCrypto AdminCrypto;

bool MeshCrypto::begin(const uint8_t psk[16]) {
  if (ready_) mbedtls_gcm_free(&gcm_);
  mbedtls_gcm_init(&gcm_);
  int rc = mbedtls_gcm_setkey(&gcm_, MBEDTLS_CIPHER_ID_AES, psk, 128);
  ready_ = (rc == 0);
  return ready_;
}

void MeshCrypto::buildNonce(const MeshHeader &h, uint8_t nonce[12]) {
  memset(nonce, 0, 12);
  nonce[0] = h.src & 0xFF;
  nonce[1] = h.src >> 8;
  nonce[2] = h.pkt_id & 0xFF;
  nonce[3] = (h.pkt_id >> 8) & 0xFF;
  nonce[4] = (h.pkt_id >> 16) & 0xFF;
  nonce[5] = (h.pkt_id >> 24) & 0xFF;
}

bool MeshCrypto::encrypt(const MeshHeader &h, const uint8_t *in, size_t len,
                         uint8_t *out, uint8_t *tag) {
  if (!ready_) return false;
  uint8_t nonce[12];
  uint8_t aad[MESH_AAD_LEN + 1];
  buildNonce(h, nonce);
  meshPackHeader(h, aad);  // packs 11 B; only the first 10 are used as AAD
  int rc = mbedtls_gcm_crypt_and_tag(&gcm_, MBEDTLS_GCM_ENCRYPT, len, nonce, 12,
                                     aad, MESH_AAD_LEN, in, out, MESH_TAG_LEN,
                                     tag);
  return rc == 0;
}

bool MeshCrypto::decrypt(const MeshHeader &h, const uint8_t *in, size_t len,
                         const uint8_t *tag, uint8_t *out) {
  if (!ready_) return false;
  uint8_t nonce[12];
  uint8_t aad[MESH_AAD_LEN + 1];
  buildNonce(h, nonce);
  meshPackHeader(h, aad);
  int rc = mbedtls_gcm_auth_decrypt(&gcm_, len, nonce, 12, aad, MESH_AAD_LEN,
                                    tag, MESH_TAG_LEN, in, out);
  return rc == 0;
}
