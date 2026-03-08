#include "provisioning.h"
#include "display.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Fall back to compile-time credentials if secrets.h exists
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define BEEP_API_KEY  ""
#endif

#define STA_TIMEOUT_MS  15000
#define DNS_PORT        53
#define AP_IP           IPAddress(192, 168, 4, 1)

static Preferences  _prefs;
static WebServer    _server(80);
static DNSServer    _dns;

// ── Credentials ───────────────────────────────────────────────────────────────

bool provisioning_has_credentials(void)
{
    _prefs.begin("wifi", true);
    bool ok = _prefs.isKey("ssid") && _prefs.getString("ssid").length() > 0;
    _prefs.end();

    // Also accept compile-time credentials
    if (!ok && strlen(WIFI_SSID) > 0) ok = true;

    return ok;
}

void provisioning_reset(void)
{
    _prefs.begin("wifi", false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[wifi] Credentials erased");
}

int provisioning_rssi_pct(void)
{
    int rssi = WiFi.RSSI();          // dBm, typically -30 (excellent) to -90 (weak)
    int pct  = constrain(2 * (rssi + 100), 0, 100);
    return pct;
}

// ── STA connect ───────────────────────────────────────────────────────────────

bool provisioning_connect_sta(void)
{
    // Prefer NVS credentials; fall back to compile-time
    String ssid, password;
    _prefs.begin("wifi", true);
    ssid     = _prefs.getString("ssid",     WIFI_SSID);
    password = _prefs.getString("password", WIFI_PASSWORD);
    _prefs.end();

    Serial.printf("[wifi] Connecting to \"%s\"...\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        display_tick();
        delay(100);
        if (millis() - start > STA_TIMEOUT_MS) {
            Serial.println("[wifi] Connection timed out");
            display_show_error("Wi-Fi connect failed.\nCheck credentials.");
            return false;
        }
    }

    Serial.printf("[wifi] Connected — IP %s  RSSI %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

// ── Captive portal ────────────────────────────────────────────────────────────

static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BeeConnect32</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#F8F9FA;font-family:sans-serif;display:flex;justify-content:center;padding:32px 16px}
  .card{background:#fff;border-radius:12px;padding:24px 20px;box-shadow:0 4px 16px rgba(0,0,0,.08);width:100%;max-width:360px}
  .logo-row{display:flex;align-items:center;gap:8px;margin-bottom:4px}
  .logo-row span{font-weight:700;font-size:16px;color:#2D2D2D}
  p{font-size:12px;color:#888;margin:0 0 16px}
  label{display:block;font-size:11px;font-weight:700;color:#555;margin-bottom:4px}
  input{width:100%;padding:9px 10px;border:1.5px solid #ccc;border-radius:6px;font-size:13px;margin-bottom:12px}
  hr{border:none;border-top:1px solid #eee;margin:12px 0}
  input:focus{outline:none;border-color:#00A896}
  button{width:100%;padding:12px;background:#F5BB00;color:#2D2D2D;border:none;border-radius:6px;font-weight:700;font-size:14px;cursor:pointer}
  .hint{font-size:10px;color:#aaa;margin-top:-8px;margin-bottom:12px}
</style>
</head>
<body>
<div class="card">
  <div class="logo-row"><span>&#x1F41D; BeeConnect32</span></div>
  <p>Enter your Wi-Fi credentials and BEEP API key.</p>
  <form method="POST" action="/save">
    <label>Wi-Fi SSID</label>
    <input name="ssid" type="text" placeholder="MyHomeNetwork" required>
    <label>Wi-Fi Password</label>
    <input name="password" type="password" placeholder="••••••••">
    <hr>
    <label>BEEP Email</label>
    <input name="beep_email" type="email" placeholder="you@example.com">
    <label>BEEP Password</label>
    <input name="beep_password" type="password" placeholder="••••••••">
    <p class="hint">Used once to fetch your API token. Credentials are not stored.</p>
    <label>BEEP Device ID</label>
    <input name="device_id" type="text" placeholder="e.g. 12345">
    <p class="hint">Found in the BEEP app under Settings &rarr; Hardware</p>
    <button type="submit">Save &amp; Connect</button>
  </form>
</div>
</body>
</html>
)HTML";

static const char SAVED_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta http-equiv="refresh" content="3;url=/">
<style>body{font-family:sans-serif;text-align:center;padding:48px;background:#F8F9FA}
h2{color:#00A896}p{color:#555}</style>
</head><body>
<h2>&#10003; Saved!</h2>
<p>Device is rebooting&hellip;</p>
</body></html>
)HTML";

static void _handle_root(void)
{
    _server.send(200, "text/html", PORTAL_HTML);
}

static void _handle_save(void)
{
    String ssid          = _server.arg("ssid");
    String pass          = _server.arg("password");
    String beep_email    = _server.arg("beep_email");
    String beep_password = _server.arg("beep_password");
    String device_id     = _server.arg("device_id");

    if (ssid.length() == 0) {
        _server.send(400, "text/plain", "SSID is required");
        return;
    }

    _prefs.begin("wifi", false);
    _prefs.putString("ssid",     ssid);
    _prefs.putString("password", pass);
    _prefs.end();

    // Store BEEP credentials for login after reboot; device_id saved directly
    _prefs.begin("beep", false);
    if (beep_email.length() > 0)    _prefs.putString("email",     beep_email);
    if (beep_password.length() > 0) _prefs.putString("password",  beep_password);
    if (device_id.length() > 0)     _prefs.putString("device_id", device_id);
    _prefs.end();

    Serial.printf("[wifi] Credentials saved for SSID: %s\n", ssid.c_str());
    _server.send(200, "text/html", SAVED_HTML);

    delay(2000);
    ESP.restart();
}

// Redirect all unknown URLs to the portal (captive portal behaviour)
static void _handle_not_found(void)
{
    _server.sendHeader("Location", "http://192.168.4.1/", true);
    _server.send(302, "text/plain", "");
}

void provisioning_start_portal(void)
{
    // Build SSID from last 4 hex digits of MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ap_ssid[24];
    snprintf(ap_ssid, sizeof(ap_ssid), "BeeConnect32-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ap_ssid);
    Serial.printf("[wifi] AP started: %s  IP: 192.168.4.1\n", ap_ssid);

    // DNS: redirect all queries to our IP
    _dns.start(DNS_PORT, "*", AP_IP);

    _server.on("/",           HTTP_GET,  _handle_root);
    _server.on("/save",       HTTP_POST, _handle_save);
    _server.onNotFound(_handle_not_found);
    _server.begin();

    Serial.println("[wifi] Captive portal running — waiting for credentials...");

    // Block until credentials submitted (save handler reboots)
    while (true) {
        _dns.processNextRequest();
        _server.handleClient();
        display_tick();
    }
}
