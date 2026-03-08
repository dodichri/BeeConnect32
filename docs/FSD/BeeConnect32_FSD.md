# Functional Specification Document — BeeConnect32
GitHub remote: https://github.com/dodichri/BeeConnect32.git

## 1. Purpose

BeeConnect32 is an embedded beehive monitoring device built on the LilyGo T-Display S3 AMOLED Plus (ESP32-S3). It continuously measures hive weight and internal temperature, displays live readings on a colour AMOLED screen, uploads sensor data to the BEEP.nl beekeeping platform via its REST API, and registers device metadata with the BEEP platform. Between measurement cycles the device enters deep sleep to conserve battery.

## 2. Branding Guide

### 2.1 Logos
- Main Logo: docs/images/BeeConnect32_logo.png
- Use Main Logo in UI and captive portal

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
- Controller: RM67162, SPI interface (initialised with `beginAMOLED_191_SPI()`)
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
    logger.h              # LOG_INFO / LOG_WARN / LOG_ERROR macros (debug-only)
    secrets_template.h    # copy to secrets.h and fill in credentials
    secrets.h             # git-ignored; holds BEEP_API_KEY, OTA_AUTH_TOKEN
  src/
    main.cpp              # setup() + loop() / boot sequence
  lib/
    sensors/              # DS18B20 + HX711 drivers
    display/              # LVGL-based UI wrapping LilyGo AMOLED library
    provisioning/         # SoftAP captive portal + STA connect
    beep_api/             # HTTPS login, sensor upload, device update
    ota/                  # Pull-based OTA from GitHub releases
    diagnostics/          # Console commands: wifi_reset, debug_metrics
  docs/
    FSD/BeeConnect32_FSD.md
    implementation_plan.md
```

### 4.2 Key Dependencies (`platformio.ini`)

| Library | Purpose |
|---------|---------|
| LilyGo AMOLED library | Board support: RM67162 display, CST816S touch, GPIO38 power-on |
| LVGL 8.x | UI rendering |
| OneWire + DallasTemperature | DS18B20 1-Wire temperature sensor |
| ArduinoJson | JSON payload construction for BEEP API |
| Preferences | NVS-backed key/value storage |
| HTTPClient + WiFiClientSecure | HTTPS requests to BEEP API and GitHub |
| WebServer + DNSServer | Captive portal HTTP server |
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
- OTA: tag commit as `vMAJOR.MINOR.PATCH`, attach `firmware.bin` as a GitHub release asset.
- Device skips OTA if running version matches latest release tag.

### 4.5 Logging
- `include/logger.h` defines `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` macros.
- All log output is active in debug builds (`build_type = debug` in `platformio.ini`).
- Log output is compiled out in release builds (`NDEBUG` defined).

## 5. Boot Flow

`setup()` runs once on power-on or wake from deep sleep:

1. Serial init (115200 baud)
2. Boot counter increment (`diag/crash_cnt`)
3. Detect wakeup cause: `ESP_SLEEP_WAKEUP_EXT0` (BOOT button) → sensor-only mode
4. Display init + splash screen (2 s)
5. BOOT button check during splash:
   - Short press (< 3 s) → sensor-only mode
   - Hold ≥ 3 s → force scale recalibration
6. If not sensor-only:
   a. Wi-Fi provisioning (SoftAP captive portal on first boot; STA connect if credentials exist)
   b. BEEP login — `POST /api/login` with stored email+password → refresh Bearer token
   c. OTA check — query GitHub releases API; download and flash if newer version available
7. Sensor init (HX711 + DS18B20)
8. Calibration check → wizard if uncalibrated or forced
9. If sensor-only: enter continuous sensor display loop (no upload, no sleep) — never returns
10. Read sensors (DS18B20 temperature + HX711 weight)
11. Display update with live sensor data and RSSI
12. BEEP device update — `POST /api/devices/multiple` with firmware version and boot count
13. BEEP sensor upload — `POST /api/sensors` with temperature, weight, RSSI
14. Hold display for 5 s
15. Deep sleep for 15 minutes (EXT0 wakeup enabled on GPIO0 for sensor-only mode)

## 6. Functional Requirements

### 6.1 Startup & Provisioning
- On first boot, enter SoftAP mode (`BeeConnect32-XXXX` where XXXX = last 4 hex digits of MAC) and serve a captive portal at `http://192.168.4.1/`.
- Portal collects: Wi-Fi SSID, Wi-Fi password, BEEP email, BEEP password, BEEP device key.
- Logo served as a separate `/logo.png` endpoint (PROGMEM, 80×68 px PNG).
- Credentials persisted via `Preferences` library; device reboots automatically after submit.
- On subsequent boots, connect in STA mode using saved credentials.

### 6.2 OTA Updates
- Pull-based: device queries `https://api.github.com/repos/dodichri/BeeConnect32/releases/latest` on each wake.
- Compares `tag_name` (e.g. `v1.2.0`) against `FIRMWARE_VERSION` using semver; skips if up to date.
- HTTP 404 from GitHub API treated as no release available (not an error).
- Downloads `firmware.bin` release asset via HTTPS (follows GitHub → S3 redirect).
- Streams firmware to `Update` library; shows OTA progress screen during download.
- Reboots on success; no downgrade — version check rejects older tags.

### 6.3 Diagnostics
- Serial console commands: `wifi_reset` (erases saved credentials and reboots), `debug_metrics` (heap, uptime, crash count).
- Persistent boot counter in `Preferences` namespace `diag`, key `crash_cnt`. Incremented at the top of `setup()` on every boot/wake.

### 6.4 Logging
- Three log levels: `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`.
- Active only in debug builds; compiled out in release builds.
- Output format: `[INFO]  message`, `[WARN]  message`, `[ERROR] message`.

### 6.5 Display UI

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
- Status bar text updated after upload via `display_set_status(msg, success)` — green on success, red on failure.

#### OTA Progress Screen
- Dark Gray background with progress bar (Teal fill) and percentage label.

#### Error Screen
- Alert Red background with `!` icon and error message.

#### Calibration Prompt
- Shown when BOOT button held during splash; prompts user to keep holding for 3 s.

### 6.6 HX711 Scale Calibration Wizard

Triggered on first boot (no saved calibration) or by holding BOOT button for 3 s during splash.

**Step 1 — Tare:** User removes all weight from scale, taps Continue. Device averages 10 raw readings as tare offset.

**Step 2 — Place weight:** User places a known reference weight. Touch UI provides `−` / `+` buttons (100 g steps, range 100 g – 50 kg) to enter the reference value. User taps Confirm.

**Step 3 — Done:** Calibration factor computed and saved to `Preferences`. Confirmation screen shows reference weight used.

### 6.7 Sensor-Only Mode

Activated by:
- Pressing BOOT button briefly (< 3 s) during the splash screen, or
- Pressing BOOT button while the device is in deep sleep (EXT0 wakeup on GPIO0)

Behaviour:
- Reads sensors and updates display every 15 seconds.
- Status bar shows "Sensor mode - reboot to exit".
- No BEEP upload, no deep sleep.
- Wi-Fi is not connected in sensor-only mode.
- Exit by rebooting the device.

### 6.8 Sensor Readings
- **DS18B20**: 1-Wire on GPIO 43. Single device on bus. Uses `DallasTemperature` library.
- **HX711**: bit-banged on GPIOs 44 (DOUT) / 45 (SCK). Averages 5 readings per measurement. Requires calibration before weight values are valid.

### 6.9 BEEP API

#### Authentication
- `POST https://api.beep.nl/api/login` with `{"email": ..., "password": ...}`
- Returns `api_token`; stored in NVS `beep/api_key`.
- Login performed on every boot (if email+password are stored) to keep token fresh.
- Email and password retained in NVS to allow re-login on each wake cycle.

#### Sensor Upload
- `POST https://api.beep.nl/api/sensors`
- Payload: `{"t": <temp_c>, "weight_kg": <weight_kg>, "rssi": <rssi_dbm>, "key": <device_key>}`
- `weight_kg` omitted if sensor read returns NaN.
- Header: `Authorization: Bearer <api_key>`

#### Device Update
- `POST https://api.beep.nl/api/devices/multiple`
- Payload: array of one device object:
  ```json
  [{"key": "...", "hardware_id": "...", "firmware_version": "...",
    "boot_count": N, "measurement_interval_min": 15, "type": "Other", "delete": false}]
  ```
- Headers: `Content-Type`, `Accept`, `Origin: https://app.beep.nl`, `Referer: https://app.beep.nl/`, `User-Agent`, `Authorization: Bearer <api_key>`
- Called on every wake cycle before sensor upload.

### 6.10 Deep Sleep
- Deep sleep for 15 minutes after each measurement and upload cycle.
- `esp_deep_sleep(15ULL * 60ULL * 1000000ULL)`
- EXT0 wakeup enabled on GPIO0 (BOOT button) to allow entry into sensor-only mode without a full power cycle.

## 7. Storage Map (`Preferences` namespaces)

| Namespace | Keys | Contents |
|-----------|------|----------|
| `wifi` | `ssid`, `password` | Wi-Fi station credentials |
| `beep` | `api_key` | BEEP Bearer token (refreshed on every boot) |
| `beep` | `email`, `password` | BEEP login credentials (retained for re-login) |
| `beep` | `device_id` | BEEP device key |
| `hx711` | `cal_factor`, `tare` | Scale calibration |
| `ota` | `lkg_part` | Last-known-good OTA partition label |
| `diag` | `crash_cnt` | Boot counter (incremented every wake) |

## 8. Configuration & Security

- Secrets stored in `include/secrets.h` (git-ignored); template in `include/secrets_template.h`.
- Optional compile-time fallback: `BEEP_API_KEY` (overridden by NVS token after login).
- BEEP credentials (email, password, device key) entered at runtime via captive portal.
- Wi-Fi credentials entered at runtime via captive portal.

## 9. Known Limitations

- No fuel gauge; battery percentage is a placeholder (always 100%).
- OTA window is narrow; device is only awake for ~10–30 s per wake cycle. OTA happens automatically at boot if a new release is available on GitHub.
- DS18B20 assumes a single device on the 1-Wire bus; multi-drop not implemented.
- Touch calibration wizard cannot be operated without a working touch screen.
- BEEP Bearer token from `/api/login` may expire between wake cycles; re-login on each boot mitigates this.
