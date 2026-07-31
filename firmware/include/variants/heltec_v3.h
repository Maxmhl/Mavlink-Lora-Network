#pragma once
// Heltec WiFi LoRa 32 V3 — ESP32-S3 + SX1262, native USB-CDC.

#define VARIANT_NAME "heltec_v3"

#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_TCXO_VOLTAGE 1.8f
#define LORA_DIO2_AS_RF_SWITCH 1

#define PIN_LED 35
#define PIN_VEXT 36  // active LOW: powers external rail (OLED etc.), unused here

// Default UART pins for the flight-controller link (node role), configurable
#define MAV_UART_TX_DEFAULT 45
#define MAV_UART_RX_DEFAULT 46
