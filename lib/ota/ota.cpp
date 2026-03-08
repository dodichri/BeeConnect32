#include "ota.h"
#include "display.h"
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

#define GITHUB_REPO    "dodichri/BeeConnect32"
#define ASSET_SUFFIX   ".bin"   // matches firmware-X.Y.Z.bin
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO "/releases/latest"
#define USER_AGENT     "BeeConnect32/" FIRMWARE_VERSION

// ── Helpers ───────────────────────────────────────────────────────────────────

// Returns true if `tag` (e.g. "v1.2.3") is strictly newer than the running firmware.
static bool _is_newer(const char *tag)
{
    const char *v = (tag && tag[0] == 'v') ? tag + 1 : tag;
    int maj = 0, min = 0, patch = 0;
    if (sscanf(v, "%d.%d.%d", &maj, &min, &patch) != 3) return false;
    if (maj  > FIRMWARE_MAJOR) return true;
    if (maj == FIRMWARE_MAJOR && min  > FIRMWARE_MINOR) return true;
    if (maj == FIRMWARE_MAJOR && min == FIRMWARE_MINOR && patch > FIRMWARE_PATCH) return true;
    return false;
}

// ── Step 1: query GitHub API ──────────────────────────────────────────────────

// Returns: 1 = ok, 0 = no releases (404), -1 = error
static int _fetch_release_info(String &out_tag, String &out_url)
{
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    https.setTimeout(10000);
    https.addHeader("User-Agent", USER_AGENT);
    https.addHeader("Accept", "application/vnd.github+json");

    if (!https.begin(client, GITHUB_API_URL)) {
        Serial.println("[ota] begin() failed");
        return -1;
    }

    int code = https.GET();
    if (code == 404) {
        Serial.println("[ota] No releases published yet — skipping OTA");
        https.end();
        return 0;
    }
    if (code != 200) {
        Serial.printf("[ota] API returned HTTP %d\n", code);
        https.end();
        return -1;
    }

    // Filter to only the fields we need — keeps heap usage low
    JsonDocument filter;
    filter["tag_name"]                          = true;
    filter["assets"][0]["name"]                 = true;
    filter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getStream(),
                                               DeserializationOption::Filter(filter));
    https.end();

    if (err) {
        Serial.printf("[ota] JSON error: %s\n", err.c_str());
        return -1;
    }

    out_tag = doc["tag_name"].as<String>();

    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
        String name = asset["name"].as<String>();
        if (name.endsWith(ASSET_SUFFIX)) {
            out_url = asset["browser_download_url"].as<String>();
            break;
        }
    }

    if (out_tag.isEmpty() || out_url.isEmpty()) {
        Serial.println("[ota] Missing tag or asset in API response");
        return -1;
    }

    Serial.printf("[ota] Latest: %s  Asset: %s\n", out_tag.c_str(), out_url.c_str());
    return 1;
}

// ── Step 2: download and flash ────────────────────────────────────────────────

static OtaResult _download_and_flash(const String &url)
{
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient dl;
    dl.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    dl.setTimeout(30000);
    dl.addHeader("User-Agent", USER_AGENT);

    if (!dl.begin(client, url)) {
        Serial.println("[ota] download begin() failed");
        return OTA_FAILED;
    }

    int code = dl.GET();
    if (code != 200) {
        Serial.printf("[ota] download HTTP %d\n", code);
        dl.end();
        return OTA_FAILED;
    }

    int total = dl.getSize();  // -1 if unknown
    Serial.printf("[ota] Firmware size: %d bytes\n", total);

    if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
        Serial.printf("[ota] Update.begin failed: %s\n", Update.errorString());
        dl.end();
        return OTA_FAILED;
    }

    WiFiClient *stream = dl.getStreamPtr();
    uint8_t buf[1024];
    int written = 0;

    while (dl.connected() && (total < 0 || written < total)) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
            if (Update.write(buf, n) != (size_t)n) {
                Serial.println("[ota] write error");
                dl.end();
                Update.abort();
                return OTA_FAILED;
            }
            written += n;
            if (total > 0) display_show_ota_progress(written * 100 / total);
        }
        delay(1);
    }

    dl.end();

    if (!Update.end(true)) {
        Serial.printf("[ota] Update.end failed: %s\n", Update.errorString());
        return OTA_FAILED;
    }

    if (!Update.isFinished()) {
        Serial.println("[ota] Update did not finish");
        return OTA_FAILED;
    }

    Serial.printf("[ota] Flash complete — %d bytes written. Rebooting...\n", written);
    display_show_ota_progress(100);
    delay(1000);
    ESP.restart();

    return OTA_UPDATED;  // unreachable
}

// ── Public API ────────────────────────────────────────────────────────────────

OtaResult ota_check_and_update(void)
{
    Serial.println("[ota] Checking GitHub for updates...");

    String tag, url;
    int rc = _fetch_release_info(tag, url);
    if (rc == 0) return OTA_UP_TO_DATE;  // no releases yet
    if (rc < 0)  return OTA_FAILED;

    if (!_is_newer(tag.c_str())) {
        Serial.printf("[ota] Up to date (v%s)\n", FIRMWARE_VERSION);
        return OTA_UP_TO_DATE;
    }

    Serial.printf("[ota] New release %s available — downloading...\n", tag.c_str());
    display_show_ota_progress(0);

    return _download_and_flash(url);
}
