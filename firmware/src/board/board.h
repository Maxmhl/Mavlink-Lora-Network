#pragma once

// Board bring-up: PMU rails (T-Beam), GPS power, LED. Must run before
// Radio::begin().
void boardInit();
void boardSetLed(bool on);
// Battery voltage in millivolts, 0 if unknown on this board.
uint16_t boardBatteryMv();

// Power the device down: PMU shutdown on T-Beam boards (wake via PWR
// button); deep sleep with button wakeup on boards without a PMU.
// Does not return.
void boardShutdown();
