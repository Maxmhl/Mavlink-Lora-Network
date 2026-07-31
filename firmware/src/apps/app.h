#pragma once
#include "../mesh/packet.h"

// Application handler interface. main.cpp instantiates apps according to the
// configured role/topics and dispatches decrypted mesh payloads by topic.
class App {
 public:
  virtual ~App() = default;
  virtual void begin() {}
  virtual void loop() {}
  // Called with the decrypted payload of a packet whose topic matches
  // topicMask(). Fragmented messages arrive as raw fragments — apps that
  // need reassembly use their own Fragmenter (see MavlinkApp).
  virtual void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {}
  virtual uint32_t topicMask() const = 0;
};
