#pragma once
#include <Arduino.h>

#include "app.h"

// Compact binary position payload (14 B, little-endian):
//   int32  lat  in 1e-7 deg
//   int32  lon  in 1e-7 deg
//   int16  alt  in m (MSL)
//   uint8  speed in km/h (saturated)
//   uint8  course in 2-deg steps
struct PositionPayload {
  int32_t lat_e7;
  int32_t lon_e7;
  int16_t alt_m;
  uint8_t speed_kmh;
  uint8_t course_2deg;
} __attribute__((packed));

class PositionApp : public App {
 public:
  void begin() override;
  void loop() override;
  void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) override;
  uint32_t topicMask() const override { return topicBit(TOPIC_POSITION); }

 private:
  uint32_t lastSend_ = 0;
};
