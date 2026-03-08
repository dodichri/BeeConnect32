#include "battery.h"
#include "logger.h"
#include <Arduino.h>
#include <Wire.h>

#define BQ25896_ADDR     0x6B
#define BQ25896_REG_BATV 0x0E

// BQ25896 register 0x0E: bits[6:0] × 20 mV + 2304 mV.
// Returns 0 when no battery is connected (register reads 0x00 → 2304 mV base).
static uint16_t _read_raw_mv()
{
    Wire.beginTransmission(BQ25896_ADDR);
    Wire.write(BQ25896_REG_BATV);
    if (Wire.endTransmission(false) != 0) {
        LOG_WARN("BQ25896: I2C write failed");
        return 0;
    }
    Wire.requestFrom((uint8_t)BQ25896_ADDR, (uint8_t)1);
    if (!Wire.available()) {
        LOG_WARN("BQ25896: no data");
        return 0;
    }
    uint8_t reg = Wire.read() & 0x7F;
    if (reg == 0) return 0;  // no battery connected
    return (uint16_t)reg * 20u + 2304u;
}

uint16_t battery_voltage_mv(void)
{
    uint16_t mv = _read_raw_mv();
    if (mv == 0) LOG_INFO("Battery: not connected (USB powered)");
    else         LOG_INFO("Battery: %u mV", mv);
    return mv;
}

int battery_pct_from_mv(uint16_t mv)
{
    if (mv == 0)    return 100;  // USB powered, no battery
    if (mv <= 3000) return 0;
    if (mv >= 4200) return 100;
    return (int)((mv - 3000UL) * 100UL / (4200UL - 3000UL));
}

int battery_pct(void)
{
    return battery_pct_from_mv(_read_raw_mv());
}
