#include "duty_cycle.h"

#include <Arduino.h>

// Ordered, non-overlapping. 869.4–869.65 is the "telemetry friendly" band:
// 10 % duty cycle and up to 500 mW ERP.
static const Eu868Band BANDS[] = {
    {863.0f, 868.0f, 0.01f, 14},
    {868.0f, 868.6f, 0.01f, 14},
    {868.7f, 869.2f, 0.001f, 14},
    {869.4f, 869.65f, 0.10f, 27},
    {869.7f, 870.0f, 0.01f, 14},
};

const Eu868Band *eu868FindBand(float freq_mhz) {
  for (const auto &b : BANDS) {
    if (freq_mhz >= b.lo_mhz && freq_mhz <= b.hi_mhz) return &b;
  }
  return nullptr;
}

void DutyCycle::begin(float freq_mhz) {
  const Eu868Band *b = eu868FindBand(freq_mhz);
  duty_ = b ? b->duty : 0.01f;
  capacity_ms_ = duty_ * 3600.0f * 1000.0f;
  tokens_ms_ = capacity_ms_;
  last_ms_ = millis();
}

void DutyCycle::refill() {
  uint32_t now = millis();
  uint32_t dt = now - last_ms_;
  last_ms_ = now;
  tokens_ms_ += (float)dt * duty_;
  if (tokens_ms_ > capacity_ms_) tokens_ms_ = capacity_ms_;
}

bool DutyCycle::canTransmit(uint32_t airtime_ms) {
  refill();
  return tokens_ms_ >= (float)airtime_ms;
}

void DutyCycle::record(uint32_t airtime_ms) {
  refill();
  tokens_ms_ -= (float)airtime_ms;
  if (tokens_ms_ < 0) tokens_ms_ = 0;
}

float DutyCycle::usagePercent() {
  refill();
  return 100.0f * (1.0f - tokens_ms_ / capacity_ms_);
}
