#include "display_ui.h"

#if HAS_DISPLAY

#include <U8g2lib.h>

#include "../board/board.h"
#include "../config/config_store.h"
#include "../mesh/router.h"
#include "../radio/radio.h"

DisplayUI Display;

#ifndef OLED_RST
#define OLED_RST U8X8_PIN_NONE
#endif

#if defined(DISPLAY_SH1106)
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL,
                                               OLED_SDA);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL,
                                                OLED_SDA);
#endif

static constexpr uint32_t DISPLAY_TIMEOUT_MS = 60000;
static constexpr uint32_t LONG_PRESS_MS = 1000;
static constexpr uint32_t DEBOUNCE_MS = 30;
static constexpr uint32_t RENDER_INTERVAL_MS = 500;

static const char *MENU_ITEMS[] = {
    "Neustart",
#if HAS_PMU
    "Ausschalten",
#else
    "Schlafen (Taste weckt)",
#endif
    "Zurueck",
};
static constexpr uint8_t MENU_COUNT = 3;

void DisplayUI::begin() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  lastActivity_ = millis();
  render();
}

// ---------------------------------------------------------------------------

static void fmtUptime(char *buf, size_t n) {
  uint32_t s = millis() / 1000;
  if (s < 3600) {
    snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60),
             (unsigned long)(s % 60));
  } else {
    snprintf(buf, n, "%luh%02lum", (unsigned long)(s / 3600),
             (unsigned long)((s % 3600) / 60));
  }
}

void DisplayUI::drawOverview() {
  char line[32];
  const NodeConfig &c = Config.cfg;

  u8g2.setFont(u8g2_font_7x14B_tf);
  snprintf(line, sizeof(line), "%s", roleName(c.role));
  u8g2.drawStr(0, 12, line);
  u8g2.setFont(u8g2_font_6x10_tf);

  snprintf(line, sizeof(line), "ID 0x%04X  FW %s", c.node_id, FW_VERSION);
  u8g2.drawStr(0, 26, line);

  char up[12];
  fmtUptime(up, sizeof(up));
  snprintf(line, sizeof(line), "Uptime %s", up);
  u8g2.drawStr(0, 38, line);

  uint16_t mv = boardBatteryMv();
  if (mv > 0) {
    snprintf(line, sizeof(line), "Akku %u.%02u V", mv / 1000, (mv % 1000) / 10);
  } else {
    snprintf(line, sizeof(line), "Akku n/a (USB)");
  }
  u8g2.drawStr(0, 50, line);

  snprintf(line, sizeof(line), "%s  %s", VARIANT_NAME,
           c.has_psk || c.role == NodeRole::ROUTER ? "" : "KEIN PSK!");
  u8g2.drawStr(0, 62, line);
}

void DisplayUI::drawRadio() {
  char line[32];
  const NodeConfig &c = Config.cfg;

  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 12, "Funk");
  u8g2.setFont(u8g2_font_6x10_tf);

  snprintf(line, sizeof(line), "%.3f MHz  SF%u", c.freq_mhz, c.sf);
  u8g2.drawStr(0, 26, line);
  snprintf(line, sizeof(line), "BW %.0f kHz  %d dBm", c.bw_khz, c.tx_dbm);
  u8g2.drawStr(0, 38, line);
  snprintf(line, sizeof(line), "Radio %s  Air %.1f%%",
           LoRaRadio.lastInitState == 0 ? "OK" : "FEHLER",
           LoRaRadio.duty.usagePercent());
  u8g2.drawStr(0, 50, line);
  snprintf(line, sizeof(line), "TX %lu  RX %lu", (unsigned long)LoRaRadio.txCount,
           (unsigned long)LoRaRadio.rxCount);
  u8g2.drawStr(0, 62, line);
}

void DisplayUI::drawNetwork() {
  char line[32];

  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 12, "Netzwerk");
  u8g2.setFont(u8g2_font_6x10_tf);

  // Up to three freshest neighbors with RSSI.
  const Neighbor *nb = Mesh.neighbors();
  int shown = 0;
  for (size_t i = 0; i < MeshRouter::NEIGHBOR_SLOTS && shown < 3; i++) {
    if (nb[i].id == 0) continue;
    snprintf(line, sizeof(line), "0x%04X %4.0fdBm %lus", nb[i].id, nb[i].rssi,
             (unsigned long)((millis() - nb[i].last_ms) / 1000));
    u8g2.drawStr(0, 26 + shown * 12, line);
    shown++;
  }
  if (shown == 0) u8g2.drawStr(0, 26, "keine Nachbarn gehoert");

  snprintf(line, sizeof(line), "Fwd %lu  Dup %lu",
           (unsigned long)Mesh.forwardedCount, (unsigned long)Mesh.dupCount);
  u8g2.drawStr(0, 62, line);
}

void DisplayUI::drawActions() {
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 12, "Aktionen");
  u8g2.setFont(u8g2_font_6x10_tf);
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    if (i == menuIndex_) {
      u8g2.drawBox(0, 18 + i * 13, 128, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, 28 + i * 13, MENU_ITEMS[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, 28 + i * 13, MENU_ITEMS[i]);
    }
  }
  u8g2.drawStr(0, 62, "kurz=weiter lang=OK");
}

void DisplayUI::render() {
  lastRender_ = millis();
  if (!displayOn_) return;
  u8g2.clearBuffer();
  switch (page_) {
    case PAGE_OVERVIEW:
      drawOverview();
      break;
    case PAGE_RADIO:
      drawRadio();
      break;
    case PAGE_NETWORK:
      drawNetwork();
      break;
    default:
      drawActions();
      break;
  }
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------

void DisplayUI::shortPress() {
  if (page_ == PAGE_ACTIONS) {
    menuIndex_ = (menuIndex_ + 1) % MENU_COUNT;
  } else {
    page_ = (page_ + 1) % PAGE_ACTIONS;  // cycle info pages only
  }
  render();
}

void DisplayUI::longPress() {
  if (page_ != PAGE_ACTIONS) {
    page_ = PAGE_ACTIONS;
    menuIndex_ = 0;
    render();
    return;
  }
  switch (menuIndex_) {
    case 0:  // reboot
      u8g2.clearBuffer();
      u8g2.drawStr(20, 36, "Neustart...");
      u8g2.sendBuffer();
      delay(300);
      ESP.restart();
      break;
    case 1:  // power off / deep sleep
      u8g2.clearBuffer();
      u8g2.drawStr(10, 36, "Schalte aus...");
      u8g2.sendBuffer();
      delay(300);
      u8g2.setPowerSave(1);
      boardShutdown();
      break;
    default:  // back
      page_ = PAGE_OVERVIEW;
      menuIndex_ = 0;
      render();
      break;
  }
}

void DisplayUI::loop() {
  uint32_t now = millis();
  bool pressed = digitalRead(BUTTON_PIN) == LOW;

  if (pressed && !btnDown_) {
    btnDown_ = true;
    longFired_ = false;
    btnDownAt_ = now;
  } else if (pressed && btnDown_ && !longFired_ &&
             now - btnDownAt_ >= LONG_PRESS_MS) {
    longFired_ = true;
    lastActivity_ = now;
    if (!displayOn_) {
      displayOn_ = true;
      u8g2.setPowerSave(0);
      render();
    } else {
      longPress();
    }
  } else if (!pressed && btnDown_) {
    btnDown_ = false;
    if (!longFired_ && now - btnDownAt_ >= DEBOUNCE_MS) {
      lastActivity_ = now;
      if (!displayOn_) {
        // First press only wakes the screen.
        displayOn_ = true;
        u8g2.setPowerSave(0);
        render();
      } else {
        shortPress();
      }
    }
  }

  if (displayOn_ && now - lastActivity_ > DISPLAY_TIMEOUT_MS) {
    displayOn_ = false;
    u8g2.setPowerSave(1);
  }
  if (displayOn_ && now - lastRender_ >= RENDER_INTERVAL_MS) render();
}

#endif  // HAS_DISPLAY
