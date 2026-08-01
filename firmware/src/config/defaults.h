#pragma once
#include <stdint.h>

#define FW_VERSION "1.1.1"

// EU868 defaults: 869.525 MHz sits in the 869.4–869.65 MHz sub-band
// (10 % duty cycle, up to 500 mW / 27 dBm ERP) — the best fit for
// telemetry streams. SF7/BW125 ≈ 5.5 kbps raw.
#define DEFAULT_FREQ_MHZ 869.525f
#define DEFAULT_BW_KHZ 125.0f
#define DEFAULT_SF 7
#define DEFAULT_CR 5  // 4/5
#define DEFAULT_TX_DBM 14
#define DEFAULT_SYNC_WORD 0x2B
#define DEFAULT_PREAMBLE_LEN 8
#define DEFAULT_HOP_LIMIT 3

#define DEFAULT_MAV_FC_BAUD 57600
#define DEFAULT_WEATHER_INTERVAL_S 60
#define DEFAULT_POSITION_INTERVAL_S 10

// Serial console / config protocol
#define CONSOLE_BAUD 115200
#define CONFIG_HANDSHAKE "+++CLMESH-CFG+++"
#define GATEWAY_CONFIG_WINDOW_MS 2000
