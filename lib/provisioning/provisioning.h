#pragma once
#include <Arduino.h>

// True if Wi-Fi credentials exist in NVS.
bool provisioning_has_credentials(void);

// Connect to saved Wi-Fi in STA mode. Returns true on success.
// Shows error screen and returns false after timeout.
bool provisioning_connect_sta(void);

// Start SoftAP captive portal (blocks until credentials are submitted,
// then saves them to NVS and reboots).
void provisioning_start_portal(void);

// Erase saved Wi-Fi credentials from NVS (used by wifi_reset command).
void provisioning_reset(void);

// RSSI as a 0–100% signal quality value. Call after connect_sta().
int provisioning_rssi_pct(void);
