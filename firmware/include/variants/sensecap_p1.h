#pragma once
// Seeed SenseCAP Solar Node P1 — internally a XIAO ESP32-S3 connected to a
// Wio-SX1262 module via the B2B connector. Permanently solar powered;
// intended role in this project: pure mesh ROUTER.

#define VARIANT_NAME "sensecap_p1"

#define LORA_SCK 7
#define LORA_MISO 8
#define LORA_MOSI 9
#define LORA_CS 41
#define LORA_DIO1 39
#define LORA_RST 42
#define LORA_BUSY 40
#define LORA_TCXO_VOLTAGE 1.8f
#define LORA_DIO2_AS_RF_SWITCH 1
#define PIN_ANT_SW 38  // HIGH = antenna switch powered

#define PIN_LED 21  // XIAO ESP32-S3 user LED
#define LED_ACTIVE_LOW 1

// XIAO D6/D7 header pins (only relevant if a P1 is used as a node)
#define MAV_UART_TX_DEFAULT 43
#define MAV_UART_RX_DEFAULT 44
