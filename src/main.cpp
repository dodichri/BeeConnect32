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
#include "battery.h"
#include "logger.h"

#define BOOT_PIN          0
#define BOOT_HOLD_MS      3000
#define SLEEP_US          (15ULL * 60ULL * 1000000ULL)  // 15 minutes
#define SENSOR_INTERVAL   15000                          // ms between reads in sensor-only mode

// ── Sensor-only read loop (no upload, no sleep) ───────────────────────────────

static void run_sensor_only_mode()
{
    LOG_INFO("Sensor-only mode — refreshing every 15 s, reboot to exit");

    while (true) {
        float temp_c   = sensors_read_temp_c();
        float weight_g = sensors_read_weight_g();
        float weight_kg = isnan(weight_g) ? 0.0f : weight_g / 1000.0f;
        if (isnan(temp_c)) temp_c = 0.0f;

        int rssi = (WiFi.status() == WL_CONNECTED) ? provisioning_rssi_pct() : 0;
        display_show_main(weight_kg, temp_c, rssi, battery_pct());
        display_set_status("Sensor mode - reboot to exit", true);

        LOG_INFO("Sensor-only: %.1f g  %.2f C", weight_g, temp_c);

        uint32_t next = millis() + SENSOR_INTERVAL;
        while (millis() < next) {
            display_tick();
            diagnostics_poll();
            delay(5);
        }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────

void setup()
{
    Serial.begin(115200);
    Serial.printf("BeeConnect32 v%s booting...\n", FIRMWARE_VERSION);
    diagnostics_increment_boot_count();

    // ── Detect wakeup cause ──
    // Button press during deep sleep → sensor-only mode
    bool sensor_only = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

    // ── Display init + splash ──
    display_init();
    display_show_splash(FIRMWARE_VERSION);

    // ── BOOT button check during splash ──
    // Short press (<3 s) → sensor-only mode
    // Long press (≥3 s) → force recalibration
    pinMode(BOOT_PIN, INPUT_PULLUP);
    bool force_cal = false;
    if (digitalRead(BOOT_PIN) == LOW) {
        display_show_cal_prompt();
        uint32_t held_since = millis();
        bool released_early = false;
        while (digitalRead(BOOT_PIN) == LOW) {
            display_tick();
            if (millis() - held_since >= BOOT_HOLD_MS) {
                force_cal = true;
                break;
            }
        }
        if (!force_cal) sensor_only = true;
    }

    if (sensor_only) LOG_INFO("Sensor-only mode activated");

    if (!sensor_only) {
        // ── Wi-Fi provisioning ──
        if (!provisioning_has_credentials()) {
            provisioning_start_portal();  // blocks until saved, then reboots
        }
        if (!provisioning_connect_sta()) {
            // Error shown by connect_sta(); continue to allow offline display
        }

        // ── BEEP login — refresh token on every boot ──
        if (WiFi.status() == WL_CONNECTED) {
            Preferences beep_prefs;
            beep_prefs.begin("beep", true);
            String beep_email    = beep_prefs.getString("email",    "");
            String beep_password = beep_prefs.getString("password", "");
            beep_prefs.end();

            if (beep_email.length() > 0 && beep_password.length() > 0) {
                LOG_INFO("Logging in to BEEP...");
                beep_login(beep_email, beep_password);
            }
        }

        // ── OTA check ──
        if (WiFi.status() == WL_CONNECTED) {
            ota_check_and_update();
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

    // ── Sensor-only mode: loop forever, no upload, no sleep ──
    if (sensor_only) {
        run_sensor_only_mode();
        // never returns
    }

    // ── Read sensors ──
    float temp_c   = sensors_read_temp_c();
    float weight_g = sensors_read_weight_g();
    float weight_kg = isnan(weight_g) ? 0.0f : weight_g / 1000.0f;
    if (isnan(temp_c)) temp_c = 0.0f;

    // ── Update display ──
    int rssi    = (WiFi.status() == WL_CONNECTED) ? provisioning_rssi_pct() : 0;
    uint16_t bat_mv = battery_voltage_mv();
    display_show_main(weight_kg, temp_c, rssi, battery_pct());

    LOG_INFO("Weight: %.1f g  Temp: %.2f C  RSSI: %d%%", weight_g, temp_c, rssi);

    // ── BEEP API upload ──
    if (WiFi.status() == WL_CONNECTED) {
        beep_update_device(bat_mv);
        BeepResult r = beep_upload(temp_c, weight_g);
        if (r == BEEP_OK) {
            display_set_status("Uploaded to BEEP", true);
        } else if (r == BEEP_NO_KEY) {
            display_set_status("No BEEP key configured", false);
        } else {
            display_set_status("BEEP upload failed", false);
        }
    } else {
        display_set_status("Offline - no upload", false);
    }

    // ── Hold screen, then deep sleep 15 min ──
    uint32_t show_until = millis() + 5000;
    while (millis() < show_until) {
        display_tick();
        diagnostics_poll();
        delay(5);
    }

    LOG_INFO("Entering deep sleep for 15 minutes");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // wake on BOOT button press
    esp_deep_sleep(SLEEP_US);
}

void loop()
{
    // Never reached — device deep sleeps at end of setup()
    display_tick();
    diagnostics_poll();
    delay(5);
}
