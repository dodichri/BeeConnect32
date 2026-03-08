#pragma once

// Logging macros — active only in debug builds (build_type = debug in platformio.ini).
// In release builds (default), all macros expand to nothing — zero runtime cost.
//
// Usage:
//   LOG_INFO("Temp: %.2f C", temp_c);
//   LOG_WARN("RSSI low: %d%%", rssi);
//   LOG_ERROR("Sensor init failed");
//
// To enable: add `build_type = debug` to your [env] in platformio.ini.

#ifndef NDEBUG
  #define LOG_INFO(fmt, ...)  Serial.printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
  #define LOG_WARN(fmt, ...)  Serial.printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
  #define LOG_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)  do {} while (0)
  #define LOG_WARN(fmt, ...)  do {} while (0)
  #define LOG_ERROR(fmt, ...) do {} while (0)
#endif
