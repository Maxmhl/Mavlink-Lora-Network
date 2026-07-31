#include "serial_console.h"

#include "../board/board.h"
#include "../config/config_store.h"
#include "../mesh/router.h"
#include "../radio/radio.h"
#include "variant.h"

SerialConsole Console;

void SerialConsole::begin(bool gatewayMode) {
  gateway_ = gatewayMode;
  bootMs_ = millis();
}

bool SerialConsole::ownsSerial() const {
  if (!gateway_) return true;
  if (configMode_) return true;
  return millis() - bootMs_ < GATEWAY_CONFIG_WINDOW_MS;
}

void SerialConsole::log(const char *msg) {
  if (gateway_ && !configMode_) return;  // never pollute the MAVLink pipe
  Serial.print("# ");
  Serial.println(msg);
}

void SerialConsole::emitEvent(const char *type,
                              std::function<void(JsonObject)> fill) {
  if (gateway_ && !configMode_) return;
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  o["evt"] = type;
  fill(o);
  serializeJson(doc, Serial);
  Serial.println();
}

void SerialConsole::loop() {
  if (!ownsSerial()) return;
  while (Serial.available()) feed((char)Serial.read());
}

void SerialConsole::feed(char c) {
  if (c == '\r') return;
  if (c != '\n') {
    if (lineLen_ < CONSOLE_LINE_MAX - 1) line_[lineLen_++] = c;
    return;
  }
  line_[lineLen_] = '\0';
  lineLen_ = 0;
  if (line_[0] != '\0') handleLine(line_);
}

void SerialConsole::reply(JsonDocument &doc) {
  serializeJson(doc, Serial);
  Serial.println();
}

void SerialConsole::handleLine(char *line) {
  if (!strcmp(line, CONFIG_HANDSHAKE)) {
    configMode_ = true;
    JsonDocument resp;
    resp["ok"] = true;
    resp["mode"] = "config";
    resp["fw"] = FW_VERSION;
    resp["variant"] = VARIANT_NAME;
    reply(resp);
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, line) != DeserializationError::Ok) {
    if (!gateway_) log("parse error");
    return;
  }
  const char *cmd = doc["cmd"] | "";
  JsonDocument resp;

  if (!strcmp(cmd, "get")) {
    cmdGet(resp);
  } else if (!strcmp(cmd, "set")) {
    cmdSet(doc.as<JsonObjectConst>(), resp);
  } else if (!strcmp(cmd, "status")) {
    cmdStatus(resp);
  } else if (!strcmp(cmd, "send")) {
    cmdSend(doc.as<JsonObjectConst>(), resp);
  } else if (!strcmp(cmd, "reboot")) {
    resp["ok"] = true;
    reply(resp);
    Serial.flush();
    delay(100);
    ESP.restart();
    return;
  } else if (!strcmp(cmd, "factory")) {
    resp["ok"] = true;
    reply(resp);
    Serial.flush();
    delay(100);
    Config.factoryReset();
    return;
  } else if (!strcmp(cmd, "exit")) {
    configMode_ = false;
    resp["ok"] = true;
    reply(resp);
    return;
  } else {
    resp["ok"] = false;
    resp["err"] = "unknown cmd";
  }
  reply(resp);
}

void SerialConsole::cmdGet(JsonDocument &resp) {
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
  resp["tx_topics"] = c.tx_topics;
  resp["rx_topics"] = c.rx_topics;
  resp["mav_peer"] = c.mav_peer;
  resp["mav_baud"] = c.mav_fc_baud;
  resp["mav_rx_pin"] = c.mav_rx_pin;
  resp["mav_tx_pin"] = c.mav_tx_pin;
  resp["weather_interval"] = c.weather_interval_s;
  resp["position_interval"] = c.position_interval_s;
}

static bool parseHex(const char *hex, uint8_t *out, size_t outLen) {
  if (strlen(hex) != outLen * 2) return false;
  for (size_t i = 0; i < outLen; i++) {
    char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
    char *end;
    out[i] = (uint8_t)strtoul(b, &end, 16);
    if (*end) return false;
  }
  return true;
}

void SerialConsole::cmdSet(JsonObjectConst args, JsonDocument &resp) {
  NodeConfig &c = Config.cfg;

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

  if (args["psk"].is<const char *>()) {
    const char *hex = args["psk"];
    if (hex[0] == '\0') {
      c.has_psk = false;
      memset(c.psk, 0, sizeof(c.psk));
    } else if (parseHex(hex, c.psk, sizeof(c.psk))) {
      c.has_psk = true;
    } else {
      resp["ok"] = false;
      resp["err"] = "psk must be 32 hex chars";
      return;
    }
  }

  bool clamped = Config.enforceLimits();
  Config.save();
  resp["ok"] = true;
  resp["clamped"] = clamped;
  resp["reboot_required"] = true;
}

void SerialConsole::cmdStatus(JsonDocument &resp) {
  resp["ok"] = true;
  resp["fw"] = FW_VERSION;
  resp["variant"] = VARIANT_NAME;
  resp["role"] = roleName(Config.cfg.role);
  resp["node_id"] = Config.cfg.node_id;
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

void SerialConsole::cmdSend(JsonObjectConst args, JsonDocument &resp) {
  uint8_t topic = args["topic"] | (uint8_t)TOPIC_GENERIC_BASE;
  uint16_t dst = args["dst"] | MESH_BROADCAST;
  const char *hex = args["hex"] | "";
  size_t n = strlen(hex) / 2;
  if (n == 0 || n > MESH_MAX_PAYLOAD) {
    resp["ok"] = false;
    resp["err"] = "bad payload";
    return;
  }
  uint8_t buf[MESH_MAX_PAYLOAD];
  if (!parseHex(hex, buf, n)) {
    resp["ok"] = false;
    resp["err"] = "bad hex";
    return;
  }
  resp["ok"] = Mesh.sendPacket(topic, dst, buf, n);
}
