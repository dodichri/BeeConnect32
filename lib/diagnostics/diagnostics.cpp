#include "diagnostics.h"
#include "provisioning.h"
#include "logger.h"
#include <Preferences.h>

#define DIAG_NS   "diag"
#define KEY_BOOTS "crash_cnt"

// ── Boot counter ──────────────────────────────────────────────────────────────

void diagnostics_increment_boot_count(void)
{
    Preferences prefs;
    prefs.begin(DIAG_NS, false);
    uint32_t cnt = prefs.getUInt(KEY_BOOTS, 0) + 1;
    prefs.putUInt(KEY_BOOTS, cnt);
    prefs.end();
    LOG_INFO("Boot count: %u", cnt);
}

// ── Command parser ────────────────────────────────────────────────────────────

static String _buf;

static void _handle_command(const String &cmd)
{
    if (cmd == "wifi_reset") {
        LOG_WARN("Erasing Wi-Fi credentials and rebooting...");
        provisioning_reset();
        delay(200);
        ESP.restart();

    } else if (cmd == "debug_metrics") {
        Preferences prefs;
        prefs.begin(DIAG_NS, true);
        uint32_t boots = prefs.getUInt(KEY_BOOTS, 0);
        prefs.end();

        LOG_INFO("── Metrics ─────────────────────────");
        LOG_INFO("Uptime:     %lu ms", millis());
        LOG_INFO("Free heap:  %u bytes", ESP.getFreeHeap());
        LOG_INFO("Min heap:   %u bytes", ESP.getMinFreeHeap());
        LOG_INFO("Boot count: %u", boots);
        LOG_INFO("────────────────────────────────────");

    } else {
        LOG_WARN("Unknown command: \"%s\"", cmd.c_str());
        LOG_INFO("Commands: wifi_reset, debug_metrics");
    }
}

void diagnostics_poll(void)
{
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            _buf.trim();
            if (_buf.length() > 0) {
                _handle_command(_buf);
                _buf = "";
            }
        } else {
            _buf += c;
        }
    }
}
