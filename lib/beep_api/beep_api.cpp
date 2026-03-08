#include "beep_api.h"
#include "logger.h"
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define BEEP_API_KEY ""
#endif

#define BEEP_API_URL       "https://api.beep.nl/api/sensors"
#define BEEP_LOGIN_URL     "https://api.beep.nl/api/login"

BeepResult beep_login(const String &email, const String &password)
{
    JsonDocument req;
    req["email"]    = email;
    req["password"] = password;
    String payload;
    serializeJson(req, payload);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(10000);
    https.addHeader("Content-Type", "application/json");

    if (!https.begin(client, BEEP_LOGIN_URL)) {
        LOG_ERROR("BEEP login begin() failed");
        return BEEP_FAILED;
    }

    int code = https.POST(payload);
    String body = https.getString();
    https.end();

    if (code != 200) {
        LOG_ERROR("BEEP login failed — HTTP %d: %s", code, body.c_str());
        return BEEP_FAILED;
    }

    JsonDocument resp;
    if (deserializeJson(resp, body) || !resp["api_token"].is<const char *>()) {
        LOG_ERROR("BEEP login: unexpected response: %s", body.c_str());
        return BEEP_FAILED;
    }

    String token = resp["api_token"].as<String>();
    Preferences prefs;
    prefs.begin("beep", false);
    prefs.putString("api_key", token);
    prefs.end();

    LOG_INFO("BEEP login OK — token stored");
    return BEEP_OK;
}


BeepResult beep_update_device(uint16_t bat_mv)
{
    Preferences prefs;
    prefs.begin("beep", true);
    String api_key   = prefs.getString("api_key",   BEEP_API_KEY);
    String device_id = prefs.getString("device_id", "");
    prefs.end();

    if (api_key.length() == 0 || device_id.length() == 0) {
        LOG_WARN("No BEEP API key or device key — skipping device update");
        return BEEP_NO_KEY;
    }

    // /api/devices/multiple expects an array of device objects
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject dev = arr.add<JsonObject>();
    Preferences diag_prefs;
    diag_prefs.begin("diag", true);
    uint32_t boot_count = diag_prefs.getUInt("crash_cnt", 0);
    diag_prefs.end();

    dev["key"]                      = device_id;
    dev["hardware_id"]              = device_id;
    dev["firmware_version"]         = FIRMWARE_VERSION;
    dev["boot_count"]               = boot_count;
    dev["measurement_interval_min"] = 15;
    dev["type"]                     = "Other";
    dev["delete"]                   = false;
    if (bat_mv > 0) dev["battery_voltage"] = bat_mv / 1000.0f;
    String payload;
    serializeJson(doc, payload);

    LOG_INFO("BEEP device update: POST /api/devices/multiple %s", payload.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(10000);
    https.addHeader("Content-Type",  "application/json");
    https.addHeader("Accept",        "application/json, text/plain, */*");
    https.addHeader("Origin",        "https://app.beep.nl");
    https.addHeader("Referer",       "https://app.beep.nl/");
    https.addHeader("User-Agent",    "Mozilla/5.0 (compatible; BeeConnect32)");
    https.addHeader("Authorization", "Bearer " + api_key);

    if (!https.begin(client, "https://api.beep.nl/api/devices/multiple")) {
        LOG_ERROR("BEEP device update begin() failed");
        return BEEP_FAILED;
    }

    int code = https.POST(payload);
    String body = https.getString();
    https.end();

    if (code == 200 || code == 201) {
        LOG_INFO("BEEP device update OK (HTTP %d)", code);
        return BEEP_OK;
    }

    LOG_ERROR("BEEP device update failed — HTTP %d: %s", code, body.c_str());
    return BEEP_FAILED;
}

BeepResult beep_upload(float temp_c, float weight_g)
{
    // Read credentials from NVS, fall back to compile-time key
    Preferences prefs;
    prefs.begin("beep", true);
    String api_key   = prefs.getString("api_key",   BEEP_API_KEY);
    String device_id = prefs.getString("device_id", "");
    prefs.end();

    if (api_key.length() == 0) {
        LOG_WARN("No BEEP API key configured — skipping upload");
        return BEEP_NO_KEY;
    }

    // Build JSON payload
    JsonDocument doc;
    doc["t"] = temp_c;
    if (!isnan(weight_g)) doc["weight_kg"] = weight_g / 1000.0f;
    doc["rssi"] = WiFi.RSSI();
    if (device_id.length() > 0) doc["key"] = device_id;

    String payload;
    serializeJson(doc, payload);

    LOG_INFO("BEEP upload: %s", payload.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(10000);
    https.addHeader("Content-Type",  "application/json");
    https.addHeader("Authorization", "Bearer " + api_key);

    if (!https.begin(client, BEEP_API_URL)) {
        LOG_ERROR("BEEP begin() failed");
        return BEEP_FAILED;
    }

    int code = https.POST(payload);
    String body = https.getString();
    https.end();

    if (code == 200 || code == 201) {
        LOG_INFO("BEEP upload OK (HTTP %d)", code);
        return BEEP_OK;
    }

    LOG_ERROR("BEEP upload failed — HTTP %d: %s", code, body.c_str());
    return BEEP_FAILED;
}
