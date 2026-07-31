#include "admin_commands.h"

#include <Arduino.h>

#include "../board/board.h"
#include "../config/config_store.h"
#include "../mesh/router.h"
#include "../radio/radio.h"
#include "variant.h"

bool adminParseHex(const char *hex, uint8_t *out, size_t outLen) {
  if (strlen(hex) != outLen * 2) return false;
  for (size_t i = 0; i < outLen; i++) {
    char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
    char *end;
    out[i] = (uint8_t)strtoul(b, &end, 16);
    if (*end) return false;
  }
  return true;
}

void adminCmdGet(JsonDocument &resp) {
  NodeConfig &c = Config.cfg;
  resp["ok"] = true;
  resp["fw"] = FW_VERSION;
  resp["variant"] = VARIANT_NAME;
  resp["configured"] = c.configured;
  resp["role"] = roleName(c.role);
  resp["node_id"] = c.node_id;
  resp["freq"] = c.freq_mhz;
  resp["bw"] = c.bw_khz;
  resp["sf"] = c.sf;
  resp["cr"] = c.cr;
  resp["tx_dbm"] = c.tx_dbm;
  resp["sync"] = c.sync_word;
  resp["hops"] = c.hop_limit;
  resp["relay"] = c.relay;
  resp["has_psk"] = c.has_psk;
  resp["has_admin_psk"] = c.has_admin_psk;
  resp["tx_topics"] = c.tx_topics;
  resp["rx_topics"] = c.rx_topics;
  resp["mav_peer"] = c.mav_peer;
  resp["mav_baud"] = c.mav_fc_baud;
  resp["mav_rx_pin"] = c.mav_rx_pin;
  resp["mav_tx_pin"] = c.mav_tx_pin;
  resp["weather_interval"] = c.weather_interval_s;
  resp["position_interval"] = c.position_interval_s;
}

void adminCmdSet(JsonObjectConst args, JsonDocument &resp, bool allowKeys) {
  NodeConfig &c = Config.cfg;

  if (!allowKeys &&
      (args["psk"].is<const char *>() || args["admin_psk"].is<const char *>())) {
    resp["ok"] = false;
    resp["err"] = "keys not settable remotely";
    return;
  }

  if (args["role"].is<const char *>()) {
    NodeRole r;
    if (!roleFromName(args["role"], r)) {
      resp["ok"] = false;
      resp["err"] = "bad role";
      return;
    }
    c.role = r;
  }
  if (args["node_id"].is<unsigned>()) c.node_id = args["node_id"];
  if (args["freq"].is<float>()) c.freq_mhz = args["freq"];
  if (args["bw"].is<float>()) c.bw_khz = args["bw"];
  if (args["sf"].is<unsigned>()) c.sf = args["sf"];
  if (args["cr"].is<unsigned>()) c.cr = args["cr"];
  if (args["tx_dbm"].is<int>()) c.tx_dbm = args["tx_dbm"];
  if (args["sync"].is<unsigned>()) c.sync_word = args["sync"];
  if (args["hops"].is<unsigned>()) c.hop_limit = args["hops"];
  if (args["relay"].is<bool>()) c.relay = args["relay"];
  if (args["tx_topics"].is<unsigned>()) c.tx_topics = args["tx_topics"];
  if (args["rx_topics"].is<unsigned>()) c.rx_topics = args["rx_topics"];
  if (args["mav_peer"].is<unsigned>()) c.mav_peer = args["mav_peer"];
  if (args["mav_baud"].is<unsigned>()) c.mav_fc_baud = args["mav_baud"];
  if (args["mav_rx_pin"].is<int>()) c.mav_rx_pin = args["mav_rx_pin"];
  if (args["mav_tx_pin"].is<int>()) c.mav_tx_pin = args["mav_tx_pin"];
  if (args["weather_interval"].is<unsigned>())
    c.weather_interval_s = args["weather_interval"];
  if (args["position_interval"].is<unsigned>())
    c.position_interval_s = args["position_interval"];

  if (allowKeys && args["psk"].is<const char *>()) {
    const char *hex = args["psk"];
    if (hex[0] == '\0') {
      c.has_psk = false;
      memset(c.psk, 0, sizeof(c.psk));
    } else if (adminParseHex(hex, c.psk, sizeof(c.psk))) {
      c.has_psk = true;
    } else {
      resp["ok"] = false;
      resp["err"] = "psk must be 32 hex chars";
      return;
    }
  }
  if (allowKeys && args["admin_psk"].is<const char *>()) {
    const char *hex = args["admin_psk"];
    if (hex[0] == '\0') {
      c.has_admin_psk = false;
      memset(c.admin_psk, 0, sizeof(c.admin_psk));
    } else if (adminParseHex(hex, c.admin_psk, sizeof(c.admin_psk))) {
      c.has_admin_psk = true;
    } else {
      resp["ok"] = false;
      resp["err"] = "admin_psk must be 32 hex chars";
      return;
    }
  }

  bool clamped = Config.enforceLimits();
  Config.save();
  resp["ok"] = true;
  resp["clamped"] = clamped;
  resp["reboot_required"] = true;
}

void adminCmdStatus(JsonDocument &resp) {
  resp["ok"] = true;
  resp["fw"] = FW_VERSION;
  resp["variant"] = VARIANT_NAME;
  resp["role"] = roleName(Config.cfg.role);
  resp["node_id"] = Config.cfg.node_id;
  resp["uptime_s"] = millis() / 1000;
  resp["radio_ok"] = (LoRaRadio.lastInitState == 0);
  resp["radio_state"] = LoRaRadio.lastInitState;
  resp["airtime_pct"] = LoRaRadio.duty.usagePercent();
  resp["tx"] = LoRaRadio.txCount;
  resp["rx"] = LoRaRadio.rxCount;
  resp["tx_drop_duty"] = LoRaRadio.dropDuty;
  resp["delivered"] = Mesh.deliveredCount;
  resp["forwarded"] = Mesh.forwardedCount;
  resp["dups"] = Mesh.dupCount;
  resp["auth_fail"] = Mesh.authFailCount;
  resp["batt_mv"] = boardBatteryMv();

  JsonArray arr = resp["neighbors"].to<JsonArray>();
  const Neighbor *nb = Mesh.neighbors();
  for (size_t i = 0; i < MeshRouter::NEIGHBOR_SLOTS; i++) {
    if (nb[i].id == 0) continue;
    JsonObject o = arr.add<JsonObject>();
    o["id"] = nb[i].id;
    o["rssi"] = nb[i].rssi;
    o["snr"] = nb[i].snr;
    o["age_s"] = (millis() - nb[i].last_ms) / 1000;
    o["packets"] = nb[i].packets;
  }
}
