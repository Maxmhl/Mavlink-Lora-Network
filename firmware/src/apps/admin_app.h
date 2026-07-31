#pragma once
#include <ArduinoJson.h>

#include "../mesh/fragment.h"
#include "app.h"

// Remote management over the air (TOPIC_ADMIN, encrypted with the admin PSK).
// Runs on EVERY role — including routers, which otherwise consume nothing.
//
// Request payload (JSON):  {"acmd":"ping"|"status"|"get"|"set"|"reboot"|
//                           "factory", ...set-fields}
// Response payload (JSON): {"resp":true,"acmd":...,"ok":...,...}
//
// Rules:
//  - ping/status/get answer unicast AND broadcast (broadcast ping = network
//    discovery; responses are spread randomly to avoid collisions)
//  - set/reboot/factory require unicast addressing
//  - key material (psk/admin_psk) is never settable remotely
class AdminApp : public App {
 public:
  AdminApp();
  void loop() override;
  void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) override;
  uint32_t topicMask() const override { return topicBit(TOPIC_ADMIN); }

  // Send a management request into the mesh (used by the serial console's
  // "remote" command; the local device acts as the USB radio bridge).
  bool sendRemote(uint16_t dst, JsonObjectConst data);

 private:
  void queueResponse(uint16_t dst, JsonDocument &doc, uint32_t delay_ms);

  Fragmenter frag_;

  // One deferred response slot (broadcast answers are delayed for
  // desynchronization; unicast goes out with minimal delay).
  uint8_t respBuf_[512];
  size_t respLen_ = 0;
  uint16_t respDst_ = 0;
  uint32_t respDue_ = 0;

  // reboot/factory are executed after the response has left the TX queue
  enum class Pending : uint8_t { NONE, REBOOT, FACTORY };
  Pending pending_ = Pending::NONE;
  uint32_t pendingAt_ = 0;
};

// Set by the constructor; used by SerialConsole for the "remote" command.
extern AdminApp *AdminInstance;
