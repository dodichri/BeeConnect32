#pragma once
#include <Arduino.h>

enum BeepResult {
    BEEP_OK,       // success
    BEEP_NO_KEY,   // no API key configured
    BEEP_FAILED,   // HTTP or network error
};

// POST to /api/login with email + password.
// On success, stores the returned token in NVS (beep/api_key) and returns BEEP_OK.
BeepResult beep_login(const String &email, const String &password);

// PUT /api/devices/{key} — update firmware_version for the device.
BeepResult beep_update_device(void);

// Upload temperature and weight to the BEEP API.
// API key is read from NVS, falling back to compile-time BEEP_API_KEY.
BeepResult beep_upload(float temp_c, float weight_g);
