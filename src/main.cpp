#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "version.h"
#include "display.h"
#include "sensors.h"
#include "provisioning.h"
#include "ota.h"
#include "diagnostics.h"
#include "beep_api.h"

#define BOOT_PIN      0
#define BOOT_HOLD_MS  3000

void setup()
{
    Serial.begin(115200);
    Serial.printf("BeeConnect32 v%s booting...\n", FIRMWARE_VERSION);
    diagnostics_increment_boot_count();

    // ── Display init + splash ──
    display_init();
    display_show_splash(FIRMWARE_VERSION);

    // ── BOOT button: hold during splash → force recalibration ──
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

    // ── Wi-Fi provisioning ──
    if (!provisioning_has_credentials()) {
        provisioning_start_portal();  // blocks until saved, then reboots
    }
    if (!provisioning_connect_sta()) {
        // Error shown by connect_sta(); continue to allow offline display
    }

    // ── BEEP login (first boot after provisioning) ──
    if (WiFi.status() == WL_CONNECTED) {
        Preferences beep_prefs;
        beep_prefs.begin("beep", true);
        String beep_email    = beep_prefs.getString("email",    "");
        String beep_password = beep_prefs.getString("password", "");
        bool   has_token     = beep_prefs.isKey("api_key");
        beep_prefs.end();

        if (!has_token && beep_email.length() > 0 && beep_password.length() > 0) {
            Serial.println("Logging in to BEEP...");
            if (beep_login(beep_email, beep_password) == BEEP_OK) {
                // Clear stored password now that we have the token
                Preferences clear_prefs;
                clear_prefs.begin("beep", false);
                clear_prefs.remove("password");
                clear_prefs.end();
            }
        }
    }

    // ── OTA check ──
    if (WiFi.status() == WL_CONNECTED) {
        ota_check_and_update();  // reboots if update flashed; returns otherwise
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
    float temp_c    = sensors_read_temp_c();
    float weight_g  = sensors_read_weight_g();
    float weight_kg = isnan(weight_g) ? 0.0f : weight_g / 1000.0f;
    if (isnan(temp_c)) temp_c = 0.0f;

    // ── Update display ──
    int rssi = (WiFi.status() == WL_CONNECTED) ? provisioning_rssi_pct() : 0;
    display_show_main(weight_kg, temp_c, rssi, 100);

    Serial.printf("Weight: %.1f g  Temp: %.2f C  RSSI: %d%%\n",
                  weight_g, temp_c, rssi);

    // ── BEEP API upload ──
    if (WiFi.status() == WL_CONNECTED) {
        beep_upload(temp_c, weight_g);
    }
}

void loop()
{
    display_tick();
    diagnostics_poll();
    delay(5);
}
