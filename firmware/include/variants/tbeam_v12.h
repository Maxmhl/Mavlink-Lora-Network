#pragma once
// LilyGo T-Beam v1.1 / v1.2 (classic ESP32) with SX1262 and NEO-6M/8M GPS.
// v1.1 uses AXP192, v1.2 uses AXP2101 — both are probed at runtime.

#define VARIANT_NAME "tbeam_v12"

#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_DIO1 33
#define LORA_RST 23
#define LORA_BUSY 32
#define LORA_TCXO_VOLTAGE 1.8f
#define LORA_DIO2_AS_RF_SWITCH 1

#define HAS_PMU 1
#define PMU_I2C_SDA 21
#define PMU_I2C_SCL 22

#define HAS_GPS 1
#define GPS_RX_PIN 34  // ESP32 RX <- GPS TX
#define GPS_TX_PIN 12
#define GPS_BAUD 9600

#define PIN_LED 4
#define LED_ACTIVE_LOW 1

#define MAV_UART_TX_DEFAULT 14
#define MAV_UART_RX_DEFAULT 13
