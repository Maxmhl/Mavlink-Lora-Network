#include "serial_console.h"

#include "admin_commands.h"
#include "../apps/admin_app.h"

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
  } else if (!strcmp(cmd, "remote")) {
    cmdRemote(doc.as<JsonObjectConst>(), resp);
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

void SerialConsole::cmdGet(JsonDocument &resp) { adminCmdGet(resp); }

void SerialConsole::cmdSet(JsonObjectConst args, JsonDocument &resp) {
  // Local USB access may set key material (allowKeys=true).
  adminCmdSet(args, resp, /*allowKeys=*/true);
}

void SerialConsole::cmdStatus(JsonDocument &resp) { adminCmdStatus(resp); }

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
  if (!adminParseHex(hex, buf, n)) {
    resp["ok"] = false;
    resp["err"] = "bad hex";
    return;
  }
  resp["ok"] = Mesh.sendPacket(topic, dst, buf, n);
}

void SerialConsole::cmdRemote(JsonObjectConst args, JsonDocument &resp) {
  if (!AdminInstance) {
    resp["ok"] = false;
    resp["err"] = "admin app not running";
    return;
  }
  if (!Config.cfg.has_admin_psk) {
    resp["ok"] = false;
    resp["err"] = "no admin_psk set";
    return;
  }
  uint16_t dst = args["dst"] | MESH_BROADCAST;
  JsonObjectConst data = args["data"];
  if (data.isNull()) {
    resp["ok"] = false;
    resp["err"] = "missing data";
    return;
  }
  // Responses arrive asynchronously as {"evt":"admin","src":...,"data":...}.
  resp["ok"] = AdminInstance->sendRemote(dst, data);
}
