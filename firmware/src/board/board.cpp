#include <Arduino.h>

#include "board.h"
#include "variant.h"

#if HAS_PMU
#include <Wire.h>
#include "XPowersAXP192.tpp"
#include "XPowersAXP2101.tpp"
static XPowersAXP2101 axp2101;
static XPowersAXP192 axp192;
static bool haveAxp2101 = false;
static bool haveAxp192 = false;
#endif

void boardInit() {
#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT);
#endif

#if HAS_PMU
  Wire.begin(PMU_I2C_SDA, PMU_I2C_SCL);
  haveAxp2101 =
      axp2101.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_I2C_SDA, PMU_I2C_SCL);
  if (haveAxp2101) {
    // T-Beam family 3.3 V aux rails (v1.2: ALDO2 = LoRa, ALDO3 = GPS; the 1W
    // board additionally gates its PA module via PIN_RADIO_LDO_EN, handled in
    // Radio::begin()).
    axp2101.setALDO2Voltage(3300);
    axp2101.enableALDO2();
    axp2101.setALDO3Voltage(3300);
    axp2101.enableALDO3();
    axp2101.setBLDO1Voltage(3300);
    axp2101.enableBLDO1();
    axp2101.enableBattVoltageMeasure();
  } else {
    // T-Beam v1.1: AXP192 (LDO2 = LoRa, LDO3 = GPS)
    haveAxp192 =
        axp192.begin(Wire, AXP192_SLAVE_ADDRESS, PMU_I2C_SDA, PMU_I2C_SCL);
    if (haveAxp192) {
      axp192.setLDO2Voltage(3300);
      axp192.enableLDO2();
      axp192.setLDO3Voltage(3300);
      axp192.enableLDO3();
      axp192.enableBattVoltageMeasure();
    }
  }
#endif

#ifdef PIN_GPS_EN
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, HIGH);
#endif
#ifdef PIN_VEXT
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, HIGH);  // Vext off (active low) — nothing attached
#endif

  delay(50);  // let rails settle before touching the radio
}

void boardSetLed(bool on) {
#ifdef PIN_LED
  digitalWrite(PIN_LED, (on ^ (bool)LED_ACTIVE_LOW) ? HIGH : LOW);
#endif
}

uint16_t boardBatteryMv() {
#if HAS_PMU
  if (haveAxp2101) return axp2101.getBattVoltage();
  if (haveAxp192) return axp192.getBattVoltage();
#endif
  return 0;
}
