#include "weather_app.h"

#include <Adafruit_BME280.h>
#include <Wire.h>

#include "../board/board.h"
#include "../config/config_store.h"
#include "../console/serial_console.h"
#include "../mesh/router.h"

static Adafruit_BME280 bme;

void WeatherApp::begin() {
  // BME280 on the standard I2C bus (0x76 or 0x77). Without a sensor the app
  // sends clearly-flagged simulated values so the data path can be tested.
  haveSensor_ = bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire);
  lastSend_ = millis() - (uint32_t)Config.cfg.weather_interval_s * 1000;
}

bool WeatherApp::sample(WeatherPayload &out) {
  if (haveSensor_) {
    out.temp_c100 = (int16_t)(bme.readTemperature() * 100.0f);
    out.hum_p100 = (uint16_t)(bme.readHumidity() * 100.0f);
    out.press_hpa10 = (uint16_t)(bme.readPressure() / 10.0f);  // Pa -> 0.1 hPa
    out.flags = 0;
  } else {
    out.temp_c100 = 2150 + (int16_t)random(-200, 200);
    out.hum_p100 = 5000 + (uint16_t)random(0, 1000);
    out.press_hpa10 = 10132;
    out.flags = 1;  // simulated
  }
  out.batt_mv = boardBatteryMv();
  return true;
}

void WeatherApp::loop() {
  if (!(Config.cfg.tx_topics & topicBit(TOPIC_WEATHER))) return;
  uint32_t interval = (uint32_t)Config.cfg.weather_interval_s * 1000;
  if (millis() - lastSend_ < interval) return;
  lastSend_ = millis();

  WeatherPayload p;
  sample(p);
  Mesh.sendPacket(TOPIC_WEATHER, MESH_BROADCAST, (const uint8_t *)&p, sizeof(p));
}

void WeatherApp::onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {
  if (len < sizeof(WeatherPayload)) return;
  WeatherPayload p;
  memcpy(&p, data, sizeof(p));
  Console.emitEvent("weather", [&](JsonObject o) {
    o["src"] = h.src;
    o["temp_c"] = p.temp_c100 / 100.0f;
    o["hum_pct"] = p.hum_p100 / 100.0f;
    o["press_hpa"] = p.press_hpa10 / 10.0f;
    o["batt_mv"] = p.batt_mv;
    o["simulated"] = (bool)(p.flags & 1);
  });
}
