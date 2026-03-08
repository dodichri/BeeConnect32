#pragma once
#include <stdint.h>

// Read battery voltage from BQ25896 register 0x0E via I2C.
// Returns voltage in millivolts, or 0 on read failure.
uint16_t battery_voltage_mv(void);

// Returns battery state of charge as 0–100%, derived from voltage.
// Returns 100 when no battery detected (USB powered).
int battery_pct(void);

// Convert a voltage (already read) to 0–100% — avoids a second I2C read.
int battery_pct_from_mv(uint16_t mv);
