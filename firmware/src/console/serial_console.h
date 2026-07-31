#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>

// JSON-lines config protocol on the USB serial port. Used by the Windows
// tool and usable by hand in any serial monitor.
//
//   {"cmd":"get"}                          -> full config
//   {"cmd":"set","role":"router",...}      -> partial update, saved to NVS
//   {"cmd":"status"}                       -> live status + neighbor table
//   {"cmd":"send","topic":16,"dst":65535,"hex":"..."}
//   {"cmd":"remote","dst":N,"data":{"acmd":"ping"|...}}   remote management
//   {"cmd":"reboot"} / {"cmd":"factory"} / {"cmd":"exit"}
//
// Roles NODE/ROUTER: console is always active (log lines start with '#',
// events are JSON lines with an "evt" key).
// Role MAVLINK_GATEWAY: the USB port is a transparent MAVLink pipe. The
// console only takes over when the line "+++CLMESH-CFG+++" arrives within
// the first 2 s after reset (the tool triggers a reset via DTR/RTS).
class SerialConsole {
 public:
  void begin(bool gatewayMode);
  void loop();

  // True while the console owns the USB serial port (gateway boot window or
  // active config session; always true for non-gateway roles).
  bool ownsSerial() const;
  bool inConfigMode() const { return configMode_; }

  void emitEvent(const char *type, std::function<void(JsonObject)> fill);
  void log(const char *msg);

 private:
  void feed(char c);
  void handleLine(char *line);
  void reply(JsonDocument &doc);
  void cmdGet(JsonDocument &resp);
  void cmdSet(JsonObjectConst args, JsonDocument &resp);
  void cmdStatus(JsonDocument &resp);
  void cmdSend(JsonObjectConst args, JsonDocument &resp);
  void cmdRemote(JsonObjectConst args, JsonDocument &resp);

  static constexpr size_t CONSOLE_LINE_MAX = 640;
  char line_[CONSOLE_LINE_MAX];
  size_t lineLen_ = 0;
  bool gateway_ = false;
  bool configMode_ = false;
  uint32_t bootMs_ = 0;
};

extern SerialConsole Console;
