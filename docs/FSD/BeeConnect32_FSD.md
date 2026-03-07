# Functional Specification Document — BeeConnect32
GitHub remote: https://github.com/dodichri/BeeConnect32.git

## 1. Purpose

BeeConnect32 is an embedded beehive monitoring device built on the LilyGo T-Display S3 AMOLED Plus (ESP32-S3). It continuously measures hive weight and internal temperature, displays live readings on a colour AMOLED screen, and periodically uploads sensor data to the BEEP.nl beekeeping platform via its REST API. Between measurement cycles the device enters deep sleep to conserve battery.

## 2. Branding Guide

### 2.1 Logos
- Main Logo: docs/images/BeeConnect32_logo.png
- Use Main Logo in UI

### 2.2 Colors
- Background: #F8F9FA

| Primary Colors | Hex |
|----------------|-----|
| Honey Yellow | #F5BB00 |
| Teal | #00A896 |

| Secondary Colors | Hex |
|------------------|-----|
| Dark Gray | #2D2D2D |
| Light Neutral | #F8F9FA |

| Accent Colors | Hex |
|---------------|-----|
| Alert Red | #E63946 |

### 2.3 Typography
- Primary Font: Montserrat Bold for headings and logo text.
- Secondary Font: Roboto Regular for body text and UI elements.
- Font Sizes: Logo Text: 24–32 pt, Headings: 18–24 pt, Body: 14–16 pt.

### 2.4 Iconography
- Use geometric, minimal icons with rounded edges.
- Sensor icons (temperature, weight) should follow the same color scheme.

### 2.5 Tone & Style
- Modern, clean, and tech-oriented with organic touches.
- Avoid overly complex graphics; keep it simple and recognizable.

## 3. Hardware Platform

### 3.1 Board
LilyGo T-Display S3 AMOLED Plus — ESP32-S3R8 (8 MB PSRAM, 16 MB Flash)

Reference: https://wiki.lilygo.cc/get_started/en/Display/T-Display-S3-AMOLED-Plus/T-Display-S3-AMOLED-Plus.html

### 3.2 Pin Assignments

| Peripheral | Signal | GPIO |
|------------|--------|------|
| RM67162 display | CS | 6 |
| RM67162 display | SCK | 47 |
| RM67162 display | D0 (MOSI/SIO) | 18 |
| RM67162 display | D1 (DC) | 7 |
| RM67162 display | D2 (QUADWP) | 48 |
| RM67162 display | D3 (QUADHD) | 17 |
| RM67162 display | RST | 17 (shared with D3) |
| CST816S touch | SDA | 3 |
| CST816S touch | SCL | 2 |
| CST816S touch | INT | 21 |
| CST816S touch | RST | none (not present on this board) |
| Peripheral power enable | PMIC_EN | 38 |
| DS18B20 temperature | 1-Wire | 43 |
| HX711 load cell | DOUT | 44 |
| HX711 load cell | SCK | 45 |
| Boot button | BOOT | 0 |

### 3.3 I2C Bus (SDA=3, SCL=2)

The touch/peripheral I2C bus is shared with other on-board devices:

| Device | Address | Purpose |
|--------|---------|---------|
| CST816S | 0x15 | Capacitive touch controller |
| BM8563 RTC | 0x51 | Real-time clock |
| Unknown | 0x6B | On-board peripheral (IMU or similar) |

GPIO38 must be driven HIGH before accessing any I2C device on this bus. The CST816S requires approximately 1000 ms after power-on before it responds on I2C.

### 3.4 Display
- Controller: RM67162, QSPI interface
- Resolution: 536 × 240 px
- LVGL 8.x used for all UI rendering via the LilyGo AMOLED library.

## 4. Build System

The project uses **PlatformIO** with the **Arduino** framework targeting the ESP32-S3.

### 4.1 Project Structure

```
BeeConnect32/
  platformio.ini          # board, framework, library dependencies
  include/
    version.h             # FIRMWARE_VERSION string + MAJOR/MINOR/PATCH defines
    secrets_template.h    # copy to secrets.h and fill in credentials
    secrets.h             # git-ignored; holds WIFI_SSID, WIFI_PASSWORD,
                          #   BEEP_API_KEY, OTA_AUTH_TOKEN
  src/
    main.cpp              # setup() + loop() / boot sequence
  lib/
    sensors/              # DS18B20 + HX711 drivers
    display/              # LVGL-based UI wrapping LilyGo AMOLED library
    provisioning/         # SoftAP captive portal + STA connect
    beep_api/             # HTTPS POST to api.beep.nl/api/sensors
    ota/                  # Authenticated OTA with SHA256 + downgrade guard
    diagnostics/          # Console commands: wifi_reset, debug_metrics
  docs/
    FSD/BeeConnect32_FSD.md
```

### 4.2 Key Dependencies (`platformio.ini`)

| Library | Purpose |
|---------|---------|
| LilyGo AMOLED library | Board support: RM67162 display, CST816S touch, GPIO38 power-on |
| LVGL 8.x | UI rendering |
| OneWire + DallasTemperature | DS18B20 1-Wire temperature sensor |
| ArduinoJson | JSON payload construction for BEEP API |
| Preferences | NVS-backed key/value storage (replaces NVS namespace API) |
| HTTPClient | HTTPS requests to BEEP API |
| WebServer | Captive portal HTTP server |
| Update | OTA firmware flashing |

### 4.3 Common Commands

```bash
pio run                        # compile
pio run --target upload        # compile + flash over USB
pio device monitor             # open serial monitor (115200 baud)
pio run --target upload && pio device monitor   # flash and monitor
```

### 4.4 Versioning
- Bump `include/version.h` for every release.
- OTA enforces semver ordering; downgrades rejected unless `?force=true`.

## 5. Boot Flow

`setup()` runs once on power-on or wake from deep sleep:

1. Serial init + crash counter increment
2. GPIO38 HIGH — enables peripheral power rail (touch, RTC, IMU)
3. LilyGo AMOLED library init (display + touch)
4. Splash screen (2 s)
5. Boot button check (hold GPIO0 for 3 s during splash → force recalibration)
6. Wi-Fi provisioning (SoftAP captive portal on first boot; STA connect if credentials exist)
7. OTA server start (HTTP POST `/ota` on port 3232)
8. HX711 init + calibration check (launch wizard if uncalibrated or recal triggered)
9. Sensor readings (DS18B20 temperature + HX711 weight)
10. Display update with live sensor data, RSSI, battery
11. BEEP API upload (HTTPS POST to `api.beep.nl/api/sensors`)
12. Deep sleep for 15 minutes

## 6. Functional Requirements

### 6.1 Startup & Provisioning
- On first boot, enter SoftAP mode (`BeeConnect32-XXXX` where XXXX = last 4 hex digits of MAC) and serve a captive portal at `http://192.168.4.1/`.
- Portal collects: Wi-Fi SSID, Wi-Fi password, BEEP API key.
- Credentials persisted via `Preferences` library and device reboots automatically.
- On subsequent boots, connect in STA mode using saved credentials.

### 6.2 OTA Updates
- HTTP server on port 3232 listens for `POST /ota` while the device is awake.
- Authenticated via `Authorization: Bearer <token>` header (`OTA_AUTH_TOKEN` in `secrets.h`).
- Optional `X-Firmware-Version` header: OTA rejects downgrades unless `?force=true`.
- Optional `X-Firmware-SHA256` header: firmware SHA256 verified before flashing.
- Uses Arduino `Update` library; last-known-good slot tracked in `Preferences`.
- Rolls back automatically on failed boot.

### 6.3 Diagnostics
- Serial console commands: `wifi_reset` (erases saved credentials), `debug_metrics` (heap, uptime, crash count).
- Persistent boot/crash counter in `Preferences`.

### 6.4 Display UI

#### Splash Screen
- Honey Yellow background.
- "BeeConnect32" title (Montserrat 28 pt, Dark Gray).
- Firmware version subtitle (Montserrat 14 pt, Dark Gray).
- Shown for 2 seconds on boot.

#### Main Screen
- Light background (`#F8F9FA`).
- Left card (Teal): weight in kg (Montserrat 28 pt).
- Right card (Honey Yellow): temperature in °C (Montserrat 28 pt).
- Bottom status bar: Wi-Fi signal quality + battery percentage.

#### OTA Progress Screen
- Dark Gray background with progress bar (Teal fill) and percentage label.

#### Error Screen
- Alert Red background with `!` icon and error message.

#### Calibration Prompt
- Shown when BOOT button held during splash; prompts user to keep holding for 3 s.

### 6.5 HX711 Scale Calibration Wizard

Triggered on first boot (no saved calibration) or by holding BOOT button for 3 s during splash.

**Step 1 — Tare:** User removes all weight from scale, taps Continue. Device averages 10 raw readings as tare offset.

**Step 2 — Place weight:** User places a known reference weight. Touch UI provides `−` / `+` buttons (100 g steps, range 100 g – 50 kg) to enter the reference value. User taps Confirm.

**Step 3 — Done:** Calibration factor computed and saved to `Preferences`. Confirmation screen shows reference weight used.

### 6.6 Sensor Readings
- **DS18B20**: 1-Wire on GPIO 43. Single device on bus. Uses `DallasTemperature` library.
- **HX711**: bit-banged on GPIOs 44 (DOUT) / 45 (SCK). Averages 5 readings per measurement. Requires calibration before weight values are valid.

### 6.7 BEEP API Upload
- HTTPS POST to `https://api.beep.nl/api/sensors`.
- Payload: `{"t": <temp_c>, "weight": <weight_g>}`.
- `Authorization: Bearer <api_key>` header.
- API key read from `Preferences` first; falls back to `BEEP_API_KEY` in `secrets.h`.

### 6.8 Deep Sleep
- Deep sleep for 15 minutes after each measurement and upload cycle.
- `esp_deep_sleep(15ULL * 60ULL * 1000000ULL)` — timer-only wakeup.

## 7. Storage Map (`Preferences` namespaces)

| Namespace | Keys | Contents |
|-----------|------|----------|
| `wifi` | `ssid`, `password` | Wi-Fi station credentials |
| `beep` | `api_key` | BEEP platform API key |
| `hx711` | `cal_factor`, `tare` | Scale calibration |
| `ota` | `lkg_part` | Last-known-good OTA partition label |
| `diag` | `crash_cnt` | Boot/crash counter |

## 8. Configuration & Security

- Secrets stored in `include/secrets.h` (git-ignored); template in `include/secrets_template.h`.
- Required secrets: `WIFI_SSID`, `WIFI_PASSWORD`, `BEEP_API_KEY`, `OTA_AUTH_TOKEN`.
- Wi-Fi credentials entered at runtime via captive portal take precedence over compile-time values.

## 9. Known Limitations

- No fuel gauge; battery percentage is a placeholder (always 100%).
- OTA server is only active while the device is awake (~10–30 s per wake cycle); OTA must be initiated immediately after device comes online.
- DS18B20 assumes a single device on the 1-Wire bus; multi-drop not implemented.
- Touch calibration wizard cannot be operated without a working touch screen.
