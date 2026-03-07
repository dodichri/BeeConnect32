#include <Arduino.h>
#include "version.h"
#include "display.h"
#include "sensors.h"

#define BOOT_PIN       0
#define BOOT_HOLD_MS   3000   // hold duration to trigger recalibration

void setup()
{
    Serial.begin(115200);
    Serial.printf("BeeConnect32 v%s booting...\n", FIRMWARE_VERSION);

    // ── Display init + splash ──
    display_init();
    display_show_splash(FIRMWARE_VERSION);

    // ── BOOT button check (held during splash → force recalibration) ──
    pinMode(BOOT_PIN, INPUT_PULLUP);
    bool force_cal = false;
    if (digitalRead(BOOT_PIN) == LOW) {
        display_show_cal_prompt();
        uint32_t held_since = millis();
        while (digitalRead(BOOT_PIN) == LOW) {
            display_tick();
            if (millis() - held_since >= BOOT_HOLD_MS) {
                force_cal = true;
                break;
            }
        }
    }

    // ── Sensors init ──
    if (!sensors_init()) {
        display_show_error("Sensor init failed.\nCheck HX711 wiring.");
        delay(5000);
    }

    // ── Calibration ──
    if (force_cal || !sensors_is_calibrated()) {
        sensors_run_calibration_wizard();
    }

    // ── Read sensors ──
    float temp_c   = sensors_read_temp_c();
    float weight_g = sensors_read_weight_g();
    float weight_kg = isnan(weight_g) ? 0.0f : weight_g / 1000.0f;
    if (isnan(temp_c)) temp_c = 0.0f;

    // ── Update display ──
    display_show_main(weight_kg, temp_c, 0, 100);

    Serial.printf("Weight: %.1f g  Temp: %.2f C\n", weight_g, temp_c);
}

void loop()
{
    display_tick();
    delay(5);
}
