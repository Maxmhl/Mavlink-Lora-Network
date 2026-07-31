#pragma once

// Board bring-up: PMU rails (T-Beam), GPS power, LED. Must run before
// Radio::begin().
void boardInit();
void boardSetLed(bool on);
// Battery voltage in millivolts, 0 if unknown on this board.
uint16_t boardBatteryMv();
