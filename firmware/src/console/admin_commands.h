#pragma once
#include <ArduinoJson.h>

// Shared management command implementations, used by both the USB serial
// console (local config) and the AdminApp (remote management over the air).
// Each function fills `resp` including the "ok" flag.

// Full config dump (never includes key material, only has_psk flags).
void adminCmdGet(JsonDocument &resp);

// Partial config update, persisted to NVS. allowKeys=false rejects the
// psk/admin_psk fields — keys must never be settable over the air.
void adminCmdSet(JsonObjectConst args, JsonDocument &resp, bool allowKeys);

// Live status: radio state, counters, airtime budget, neighbors, battery.
void adminCmdStatus(JsonDocument &resp);

// "0a1b..." -> bytes; returns false on length/format mismatch.
bool adminParseHex(const char *hex, uint8_t *out, size_t outLen);
