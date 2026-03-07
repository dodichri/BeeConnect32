#pragma once
#include <Arduino.h>

enum OtaResult {
    OTA_UP_TO_DATE,  // running latest release
    OTA_UPDATED,     // flashed successfully — device rebooted
    OTA_FAILED,      // download or flash error
};

// Check GitHub releases for a newer firmware.bin asset.
// If found: shows OTA progress screen, flashes, reboots. Never returns OTA_UPDATED.
// If up to date or error: returns immediately.
OtaResult ota_check_and_update(void);
