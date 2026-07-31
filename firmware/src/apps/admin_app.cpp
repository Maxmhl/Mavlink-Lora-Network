#include "admin_app.h"

#include "../board/board.h"
#include "../config/config_store.h"
#include "../console/admin_commands.h"
#include "../console/serial_console.h"

AdminApp *AdminInstance = nullptr;

AdminApp::AdminApp() { AdminInstance = this; }

bool AdminApp::sendRemote(uint16_t dst, JsonObjectConst data) {
  char buf[512];
  size_t n = serializeJson(data, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) return false;
  return frag_.send(TOPIC_ADMIN, dst, (const uint8_t *)buf, n);
}

void AdminApp::queueResponse(uint16_t dst, JsonDocument &doc,
                             uint32_t delay_ms) {
  respLen_ = serializeJson(doc, respBuf_, sizeof(respBuf_));
  if (respLen_ == 0 || respLen_ >= sizeof(respBuf_)) {
    respLen_ = 0;
    return;
  }
  respDst_ = dst;
  respDue_ = millis() + delay_ms;
}

void AdminApp::onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {
  size_t msgLen = 0;
  const uint8_t *msg = frag_.feed(h, data, len, &msgLen);
  if (!msg || msgLen == 0) return;

  JsonDocument doc;
  if (deserializeJson(doc, msg, msgLen) != DeserializationError::Ok) return;

  // Responses to requests WE sent: surface them on the serial console for
  // the Windows tool ({"evt":"admin","src":...,"data":{...}}).
  if (doc["resp"].as<bool>()) {
    Console.emitEvent("admin", [&](JsonObject o) {
      o["src"] = h.src;
      o["data"] = doc.as<JsonObjectConst>();
    });
    return;
  }

  const char *acmd = doc["acmd"] | "";
  bool unicast = (h.dst != MESH_BROADCAST);
  float reqRssi = Mesh.lastRssi;
  float reqSnr = Mesh.lastSnr;

  JsonDocument resp;
  resp["resp"] = true;
  resp["acmd"] = acmd;

  if (!strcmp(acmd, "ping")) {
    resp["ok"] = true;
    resp["fw"] = FW_VERSION;
    resp["role"] = roleName(Config.cfg.role);
    resp["node_id"] = Config.cfg.node_id;
    resp["uptime_s"] = millis() / 1000;
    resp["batt_mv"] = boardBatteryMv();
    resp["rssi"] = reqRssi;  // how this device heard the request
    resp["snr"] = reqSnr;
  } else if (!strcmp(acmd, "status")) {
    adminCmdStatus(resp);
    resp["rssi"] = reqRssi;
    resp["snr"] = reqSnr;
  } else if (!strcmp(acmd, "get")) {
    adminCmdGet(resp);
  } else if (!strcmp(acmd, "set")) {
    if (!unicast) {
      resp["ok"] = false;
      resp["err"] = "set requires unicast";
    } else {
      adminCmdSet(doc.as<JsonObjectConst>(), resp, /*allowKeys=*/false);
    }
  } else if (!strcmp(acmd, "reboot") || !strcmp(acmd, "factory")) {
    if (!unicast) {
      resp["ok"] = false;
      resp["err"] = "requires unicast";
    } else {
      resp["ok"] = true;
      pending_ = strcmp(acmd, "factory") ? Pending::REBOOT : Pending::FACTORY;
      pendingAt_ = millis() + 2500;  // let the response drain first
    }
  } else {
    resp["ok"] = false;
    resp["err"] = "unknown acmd";
  }

  // Broadcast requests: spread responses over 100-1500 ms so a discovery
  // ping does not trigger a collision storm.
  uint32_t delay_ms = unicast ? 0 : random(100, 1500);
  queueResponse(h.src, resp, delay_ms);
}

void AdminApp::loop() {
  uint32_t now = millis();
  if (respLen_ > 0 && (int32_t)(now - respDue_) >= 0) {
    frag_.send(TOPIC_ADMIN, respDst_, respBuf_, respLen_);
    respLen_ = 0;
  }
  if (pending_ != Pending::NONE && (int32_t)(now - pendingAt_) >= 0) {
    if (pending_ == Pending::FACTORY) {
      Config.factoryReset();  // clears NVS and restarts
    } else {
      ESP.restart();
    }
  }
}
