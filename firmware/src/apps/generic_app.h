#pragma once
#include "app.h"

// Extensibility hook: topics 0x10..0x1F carry opaque user payloads. Received
// packets are surfaced as JSON events on the console; sending is available
// through the console "send" command ({"cmd":"send","topic":16,...}).
class GenericApp : public App {
 public:
  void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) override;
  uint32_t topicMask() const override {
    uint32_t m = 0;
    for (uint8_t t = TOPIC_GENERIC_BASE; t < TOPIC_GENERIC_BASE + 16; t++)
      m |= topicBit(t);
    return m;
  }
};
