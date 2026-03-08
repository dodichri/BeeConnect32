#pragma once
#include <Arduino.h>

// Call once in setup() to increment the boot/crash counter in NVS.
void diagnostics_increment_boot_count(void);

// Call in loop() to poll Serial for diagnostic commands.
// Commands:
//   wifi_reset     — erase Wi-Fi credentials and reboot
//   debug_metrics  — print heap, uptime, and crash counter to Serial
void diagnostics_poll(void);
