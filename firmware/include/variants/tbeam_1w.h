#pragma once
// LilyGo T-Beam 1W — ESP32-S3 + SX1262 with 1 W PA front end, L76K GNSS,
// AXP2101 PMU. Pins per Xinyuan-LilyGO/LilyGo-LoRa-Series utilities.h.
// The PA module is powered via GPIO40 (LDO enable); GPIO21 powers the RX LNA
// (must be HIGH in RX, LOW in TX — handled via RadioLib RF-switch control).
// DIO2 drives the TX side of the antenna switch.

#define VARIANT_NAME "tbeam_1w"

#define LORA_SCK 13
#define LORA_MISO 12
#define LORA_MOSI 11
#define LORA_CS 15
#define LORA_DIO1 1
#define LORA_RST 3
#define LORA_BUSY 38
#define LORA_TCXO_VOLTAGE 1.8f
#define LORA_DIO2_AS_RF_SWITCH 1
#define PIN_RADIO_LDO_EN 40  // HIGH = radio/PA module powered
#define PIN_RADIO_RX_EN 21   // HIGH in RX (LNA on), LOW in TX

#define HAS_PMU 1
#define PMU_I2C_SDA 8
#define PMU_I2C_SCL 9

#define HAS_GPS 1
#define GPS_RX_PIN 5  // ESP32 RX <- GPS TX
#define GPS_TX_PIN 6
#define GPS_BAUD 9600
#define PIN_GPS_EN 16  // HIGH = GNSS powered

#define PIN_LED 18

#define MAV_UART_TX_DEFAULT 43
#define MAV_UART_RX_DEFAULT 44
