#pragma once
#include <Arduino.h>

#include "app.h"

// Compact binary weather payload (10 B, little-endian):
//   int16  temperature  in 0.01 °C
//   uint16 humidity     in 0.01 %rH
//   uint16 pressure     in 0.1 hPa
//   uint16 battery      in mV (0 = unknown)
//   uint16 flags        bit0 = simulated data (no sensor found)
struct WeatherPayload {
  int16_t temp_c100;
  uint16_t hum_p100;
  uint16_t press_hpa10;
  uint16_t batt_mv;
  uint16_t flags;
} __attribute__((packed));

class WeatherApp : public App {
 public:
  void begin() override;
  void loop() override;
  void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) override;
  uint32_t topicMask() const override { return topicBit(TOPIC_WEATHER); }

 private:
  bool sample(WeatherPayload &out);
  bool haveSensor_ = false;
  uint32_t lastSend_ = 0;
};
