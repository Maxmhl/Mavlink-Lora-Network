#include "radio.h"

#include <SPI.h>

#include "variant.h"

Radio LoRaRadio;

static SPIClass loraSpi(HSPI);
static volatile bool rxFlag = false;

static void IRAM_ATTR onDio1() { rxFlag = true; }

bool Radio::begin(const NodeConfig &cfg) {
#ifdef PIN_ANT_SW
  pinMode(PIN_ANT_SW, OUTPUT);
  digitalWrite(PIN_ANT_SW, HIGH);
#endif
#ifdef PIN_RADIO_LDO_EN
  pinMode(PIN_RADIO_LDO_EN, OUTPUT);
  digitalWrite(PIN_RADIO_LDO_EN, HIGH);
  delay(10);
#endif

  loraSpi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  static Module mod(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSpi);
  static SX1262 sx(&mod);
  lora_ = &sx;

  int16_t state = lora_->begin(cfg.freq_mhz, cfg.bw_khz, cfg.sf, cfg.cr,
                               cfg.sync_word, cfg.tx_dbm, DEFAULT_PREAMBLE_LEN,
                               LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    // Some module revisions run from a plain XTAL — retry without TCXO.
    state = lora_->begin(cfg.freq_mhz, cfg.bw_khz, cfg.sf, cfg.cr,
                         cfg.sync_word, cfg.tx_dbm, DEFAULT_PREAMBLE_LEN, 0);
  }
  lastInitState = state;
  if (state != RADIOLIB_ERR_NONE) return false;

#if LORA_DIO2_AS_RF_SWITCH
  lora_->setDio2AsRfSwitch(true);
#endif
#ifdef PIN_RADIO_RX_EN
  // T-Beam 1W: GPIO21 powers the RX LNA — HIGH in RX, LOW in TX.
  lora_->setRfSwitchPins(PIN_RADIO_RX_EN, RADIOLIB_NC);
#endif
  lora_->setCRC(2);
  lora_->setDio1Action(onDio1);

  duty.begin(cfg.freq_mhz);
  ok_ = (lora_->startReceive() == RADIOLIB_ERR_NONE);
  return ok_;
}

size_t Radio::receive(uint8_t *buf, size_t maxlen, float *rssi, float *snr) {
  if (!ok_ || !rxFlag) return 0;
  rxFlag = false;
  size_t len = lora_->getPacketLength();
  if (len == 0 || len > maxlen) {
    lora_->startReceive();
    return 0;
  }
  int16_t state = lora_->readData(buf, len);
  if (rssi) *rssi = lora_->getRSSI();
  if (snr) *snr = lora_->getSNR();
  lora_->startReceive();
  if (state != RADIOLIB_ERR_NONE) return 0;
  rxCount++;
  return len;
}

uint32_t Radio::airtimeMs(size_t len) {
  if (!ok_) return 0;
  return (uint32_t)(lora_->getTimeOnAir(len) / 1000);
}

bool Radio::send(const uint8_t *data, size_t len) {
  if (!ok_ || len == 0 || len > MESH_MAX_FRAME) return false;

  uint32_t airtime = airtimeMs(len);
  if (!duty.canTransmit(airtime)) {
    dropDuty++;
    return false;
  }

  // Basic CSMA: back off while channel activity is detected.
  for (int attempt = 0; attempt < 3; attempt++) {
    if (rxFlag) return false;  // finish pending RX first, caller retries
    int16_t cad = lora_->scanChannel();
    if (cad == RADIOLIB_CHANNEL_FREE) break;
    delay(random(20, 80));
  }

  int16_t state = lora_->transmit((uint8_t *)data, len);
  rxFlag = false;  // DIO1 fires on TX-done too
  lora_->startReceive();
  if (state != RADIOLIB_ERR_NONE) return false;
  duty.record(airtime);
  txCount++;
  return true;
}
