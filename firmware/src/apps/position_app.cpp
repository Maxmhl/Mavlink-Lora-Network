#include "position_app.h"

#include "../config/config_store.h"
#include "../console/serial_console.h"
#include "../mesh/router.h"
#include "variant.h"

#if HAS_GPS
#include <TinyGPSPlus.h>
static TinyGPSPlus gps;
static HardwareSerial gpsSerial(2);
#endif

void PositionApp::begin() {
#if HAS_GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif
}

void PositionApp::loop() {
#if HAS_GPS
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  if (!(Config.cfg.tx_topics & topicBit(TOPIC_POSITION))) return;
  uint32_t interval = (uint32_t)Config.cfg.position_interval_s * 1000;
  if (millis() - lastSend_ < interval) return;
  if (!gps.location.isValid()) return;
  lastSend_ = millis();

  PositionPayload p;
  p.lat_e7 = (int32_t)(gps.location.lat() * 1e7);
  p.lon_e7 = (int32_t)(gps.location.lng() * 1e7);
  p.alt_m = (int16_t)gps.altitude.meters();
  double kmh = gps.speed.kmph();
  p.speed_kmh = kmh > 255 ? 255 : (uint8_t)kmh;
  p.course_2deg = (uint8_t)(gps.course.deg() / 2.0);
  Mesh.sendPacket(TOPIC_POSITION, MESH_BROADCAST, (const uint8_t *)&p, sizeof(p));
#endif
}

void PositionApp::onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {
  if (len < sizeof(PositionPayload)) return;
  PositionPayload p;
  memcpy(&p, data, sizeof(p));
  Console.emitEvent("position", [&](JsonObject o) {
    o["src"] = h.src;
    o["lat"] = p.lat_e7 / 1e7;
    o["lon"] = p.lon_e7 / 1e7;
    o["alt_m"] = p.alt_m;
    o["speed_kmh"] = p.speed_kmh;
    o["course_deg"] = p.course_2deg * 2;
  });
}
