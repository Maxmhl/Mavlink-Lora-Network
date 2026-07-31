#include <Arduino.h>

#include "apps/admin_app.h"
#include "apps/generic_app.h"
#include "apps/mavlink_app.h"
#include "apps/position_app.h"
#include "apps/weather_app.h"
#include "board/board.h"
#include "config/config_store.h"
#include "console/serial_console.h"
#include "crypto/crypto.h"
#include "mesh/router.h"
#include "radio/radio.h"
#include "variant.h"

static App *apps[6];
static size_t nApps = 0;
static MavlinkApp *mavApp = nullptr;
static bool isGateway = false;

static void addApp(App *a) {
  if (nApps < sizeof(apps) / sizeof(apps[0])) apps[nApps++] = a;
}

void setup() {
  Serial.begin(CONSOLE_BAUD);
  delay(200);  // give native USB-CDC a moment to enumerate

  boardInit();
  Config.load();
  const NodeConfig &c = Config.cfg;
  isGateway = (c.role == NodeRole::MAVLINK_GATEWAY);

  Console.begin(isGateway);

  // Routers deliberately never receive the DATA PSK; everyone else needs it
  // for end-to-end payload encryption. The admin PSK (remote management)
  // goes to every role — including routers.
  if (c.role != NodeRole::ROUTER && c.has_psk) Crypto.begin(c.psk);
  if (c.has_admin_psk) AdminCrypto.begin(c.admin_psk);

  bool radioOk = LoRaRadio.begin(c);
  Mesh.begin(&Config.cfg);

  // Remote management runs on every role, routers included.
  addApp(new AdminApp());

  switch (c.role) {
    case NodeRole::ROUTER:
      break;  // forwarding only — no data applications, no data crypto
    case NodeRole::MAVLINK_GATEWAY:
      mavApp = new MavlinkApp(true);
      addApp(mavApp);
      break;
    case NodeRole::NODE: {
      uint32_t topics = c.tx_topics | c.rx_topics;
      if (topics & topicBit(TOPIC_MAVLINK)) {
        mavApp = new MavlinkApp(false);
        addApp(mavApp);
      }
      if (topics & topicBit(TOPIC_WEATHER)) addApp(new WeatherApp());
      if (HAS_GPS || (c.rx_topics & topicBit(TOPIC_POSITION)))
        addApp(new PositionApp());
      addApp(new GenericApp());
      break;
    }
  }

  Mesh.onDeliver([](const MeshHeader &h, const uint8_t *data, size_t len) {
    // Gateway in config mode: hold MAVLink off the USB port.
    for (size_t i = 0; i < nApps; i++) {
      if (!(apps[i]->topicMask() & topicBit(h.topic))) continue;
      if (isGateway && apps[i] == mavApp && Console.inConfigMode()) continue;
      apps[i]->onPacket(h, data, len);
    }
  });

  for (size_t i = 0; i < nApps; i++) apps[i]->begin();

  char banner[128];
  snprintf(banner, sizeof(banner),
           "CLMESH %s %s role=%s id=0x%04X freq=%.3f sf=%u radio=%s(%d)",
           FW_VERSION, VARIANT_NAME, roleName(c.role), c.node_id, c.freq_mhz,
           c.sf, radioOk ? "ok" : "FAIL", LoRaRadio.lastInitState);
  Console.log(banner);
  if (c.role != NodeRole::ROUTER && !c.has_psk)
    Console.log("WARNING: no PSK set — payloads are NOT encrypted");
  if (!c.has_admin_psk)
    Console.log("NOTE: no admin PSK set — remote management disabled");
}

void loop() {
  Mesh.loop();
  Console.loop();

  bool consoleOwnsSerial = isGateway && Console.ownsSerial();
  for (size_t i = 0; i < nApps; i++) {
    // The gateway MAVLink app must not read USB bytes while the console
    // owns the port (config window / config session).
    if (apps[i] == mavApp && consoleOwnsSerial) continue;
    apps[i]->loop();
  }

  // Heartbeat: routers blink slowly, nodes on activity.
  static uint32_t lastBlink = 0;
  static bool led = false;
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    led = !led;
    boardSetLed(led);
  }
}
