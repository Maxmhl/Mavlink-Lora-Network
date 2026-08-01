#pragma once
#include <Arduino.h>

#include "variant.h"

#if HAS_DISPLAY

// On-device OLED UI (Heltec V3: SSD1306, T-Beam: SSD1306/SH1106) with a
// single-button interface:
//   short press  — next page / next menu entry
//   long press   — info pages: jump to the action menu
//                  action menu: execute the selected entry
// Pages: overview, radio, network, actions (reboot / power off / back).
// The display blanks after 60 s; any press wakes it first.
class DisplayUI {
 public:
  void begin();
  void loop();

 private:
  enum Page : uint8_t { PAGE_OVERVIEW, PAGE_RADIO, PAGE_NETWORK, PAGE_ACTIONS,
                        PAGE_COUNT };

  void render();
  void drawOverview();
  void drawRadio();
  void drawNetwork();
  void drawActions();
  void shortPress();
  void longPress();

  uint8_t page_ = PAGE_OVERVIEW;
  uint8_t menuIndex_ = 0;
  bool displayOn_ = true;
  uint32_t lastActivity_ = 0;
  uint32_t lastRender_ = 0;

  // button state machine
  bool btnDown_ = false;
  bool longFired_ = false;
  uint32_t btnDownAt_ = 0;
};

extern DisplayUI Display;

#endif  // HAS_DISPLAY
