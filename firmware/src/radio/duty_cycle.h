#pragma once
#include <stdint.h>

// EU868 SRD sub-bands (simplified, per ETSI EN 300 220 / German AFuV usage):
// duty cycle limit + max ERP. TX is gated by a token bucket whose refill
// rate equals the duty-cycle fraction.
struct Eu868Band {
  float lo_mhz;
  float hi_mhz;
  float duty;      // 0.10 = 10 %
  int8_t max_dbm;  // max ERP
};

// Returns nullptr if freq is outside all allowed bands.
const Eu868Band *eu868FindBand(float freq_mhz);

class DutyCycle {
 public:
  void begin(float freq_mhz);
  bool canTransmit(uint32_t airtime_ms);
  void record(uint32_t airtime_ms);
  // 0..100: share of the hourly airtime budget currently used up
  float usagePercent();

 private:
  void refill();
  float duty_ = 0.01f;
  float capacity_ms_ = 36000.0f;  // duty * 1 h
  float tokens_ms_ = 36000.0f;
  uint32_t last_ms_ = 0;
};
