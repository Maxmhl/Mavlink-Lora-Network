#include "generic_app.h"

#include "../console/serial_console.h"

void GenericApp::onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {
  static const char *HEXCHARS = "0123456789abcdef";
  String hex;
  hex.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    hex += HEXCHARS[data[i] >> 4];
    hex += HEXCHARS[data[i] & 0x0F];
  }
  Console.emitEvent("generic", [&](JsonObject o) {
    o["src"] = h.src;
    o["topic"] = h.topic;
    o["hex"] = hex;
  });
}
