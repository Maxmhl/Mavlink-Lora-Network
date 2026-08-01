#pragma once
#include <RadioLib.h>

#include "../config/config_store.h"
#include "duty_cycle.h"

// SX1262 wrapper: interrupt-driven RX, blocking TX with CSMA (CAD) and
// EU868 duty-cycle gating.
class Radio {
 public:
  bool begin(const NodeConfig &cfg);
  // Returns received length, 0 if nothing pending. rssi/snr filled on RX.
  size_t receive(uint8_t *buf, size_t maxlen, float *rssi, float *snr);
  // CSMA + duty-cycle-gated blocking transmit. false if dropped.
  bool send(const uint8_t *data, size_t len);

  uint32_t airtimeMs(size_t len);
  // Put the SX1262 into sleep mode (used before device shutdown/deep sleep).
  void sleep();
  DutyCycle duty;
  uint32_t txCount = 0;
  uint32_t rxCount = 0;
  uint32_t dropDuty = 0;
  int16_t lastInitState = 0;

 private:
  SX1262 *lora_ = nullptr;
  bool ok_ = false;
};

extern Radio LoRaRadio;
