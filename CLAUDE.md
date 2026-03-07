# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BeeConnect32 is an embedded beehive monitoring device firmware for the **LilyGo T-Display S3 AMOLED Plus (ESP32-S3)**. It reads hive weight (HX711 load cell) and temperature (DS18B20), displays them on the onboard AMOLED screen, uploads data to the BEEP.nl beekeeping API, then deep-sleeps for 15 minutes.

## Build System

**PlatformIO** with the Arduino framework targeting ESP32-S3.

```bash
~/.platformio-venv/bin/pio run                                          # compile
~/.platformio-venv/bin/pio run --target upload                          # compile + flash via USB
~/.platformio-venv/bin/pio device monitor                               # serial monitor (115200 baud)
~/.platformio-venv/bin/pio run --target upload && ~/.platformio-venv/bin/pio device monitor  # flash and monitor
```

`pio` is available as a plain command if `~/.platformio-venv/bin` is on PATH (added to `~/.bashrc`). Open a new shell or run `source ~/.bashrc` to activate.

## Project Structure

```
BeeConnect32/
  platformio.ini          # board config, framework, library dependencies
  include/
    version.h             # FIRMWARE_VERSION + MAJOR/MINOR/PATCH defines
    secrets_template.h    # copy to secrets.h and fill credentials
    secrets.h             # git-ignored; WIFI_SSID, WIFI_PASSWORD, BEEP_API_KEY, OTA_AUTH_TOKEN
  src/
    main.cpp              # setup() + loop() / full boot sequence
  lib/
    sensors/              # DS18B20 (GPIO 43, 1-Wire) + HX711 (GPIO 44 DOUT / 45 SCK)
    display/              # LVGL 8.x UI wrapping LilyGo AMOLED library
    provisioning/         # SoftAP captive portal + STA connect
    beep_api/             # HTTPS POST to api.beep.nl/api/sensors
    ota/                  # Authenticated OTA with SHA256 verification + downgrade guard
    diagnostics/          # Serial console commands: wifi_reset, debug_metrics
```

## Key Architecture Points

### Boot Flow (`src/main.cpp` `setup()`)
1. GPIO38 HIGH — must happen before any I2C access (powers touch, RTC, IMU)
2. LilyGo AMOLED init → splash screen (2 s)
3. BOOT button check: hold GPIO0 for 3 s during splash → force scale recalibration
4. Wi-Fi provisioning (SoftAP captive portal on first boot; STA if credentials exist)
5. OTA HTTP server start on port 3232
6. HX711 calibration check → wizard if uncalibrated
7. Sensor readings → display update → BEEP API upload → deep sleep 15 min

### Hardware Constraints
- **GPIO38 must be HIGH** before accessing any I2C device. CST816S needs ~1000 ms after power-on.
- Display: RM67162 **SPI** controller (not QSPI), 536×240 px. All UI via LVGL 8.x.
- Init with `beginAMOLED_191_SPI()` — the Plus variant has both CST816S (0x15) and BM8563 RTC (0x51) on the I2C bus, which the library uses to select the SPI code path. `beginAMOLED_191()` (QSPI) produces a blank screen.
- I2C bus (SDA=3, SCL=2) shared by CST816S touch (0x15), BM8563 RTC (0x51), and an unknown device (0x6B).

### Storage (`Preferences` NVS namespaces)
| Namespace | Keys | Contents |
|-----------|------|----------|
| `wifi` | `ssid`, `password` | Station credentials |
| `beep` | `api_key` | BEEP API key (overrides secrets.h) |
| `hx711` | `cal_factor`, `tare` | Scale calibration |
| `ota` | `lkg_part` | Last-known-good OTA partition |
| `diag` | `crash_cnt` | Boot/crash counter |

### BEEP API
- `POST https://api.beep.nl/api/sensors`
- Payload: `{"t": <°C>, "weight": <grams>}`
- Header: `Authorization: Bearer <api_key>`
- API key from `Preferences` namespace `beep` takes precedence over compile-time `BEEP_API_KEY`.

### OTA
- POST to `http://<device-ip>:3232/ota` while device is awake (~10–30 s per cycle)
- Requires `Authorization: Bearer <OTA_AUTH_TOKEN>`
- Optional `X-Firmware-Version` (rejects downgrades unless `?force=true`) and `X-Firmware-SHA256` headers
- Rollback on failed boot via last-known-good partition in `Preferences`

### Versioning
- Bump `include/version.h` for every release (MAJOR/MINOR/PATCH + FIRMWARE_VERSION string)

### Branding / UI Colors
- Background: `#F8F9FA`, Honey Yellow: `#F5BB00`, Teal: `#00A896`, Dark Gray: `#2D2D2D`, Alert Red: `#E63946`
- Fonts: Montserrat Bold (headings), Roboto Regular (body)

### Known Limitations
- Battery percentage is a placeholder (always 100% — no fuel gauge)
- OTA window is narrow; must be triggered immediately after device wakes
- Single DS18B20 assumed on 1-Wire bus (no multi-drop support)
