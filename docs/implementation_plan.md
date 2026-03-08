# BeeConnect32 — Implementation Plan

## Phase 0: Development Environment Setup ✅

### 0.1 Prerequisites
- **OS**: Linux, macOS, or Windows with WSL2
- **Python**: 3.8+ (required by PlatformIO)
- **Git**
- **USB-C cable** capable of data transfer (not charge-only) for flashing the T-Display S3

### 0.2 Install PlatformIO Core (CLI)

```bash
pip install --user platformio
```

Add `~/.local/bin` (Linux/macOS) to PATH if not already present:
```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
```

Verify:
```bash
pio --version
```

### 0.3 Install USB Driver (Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG dialout $USER
```

### 0.4 Clone Repository and First Build

```bash
git clone https://github.com/dodichri/BeeConnect32.git
cd BeeConnect32
cp include/secrets_template.h include/secrets.h
# Edit include/secrets.h
pio run
```

### 0.5 Flash and Verify

```bash
pio run --target upload && pio device monitor
```

---

## Phase 1: Project Scaffold ✅

**Goal**: Compile-clean skeleton with correct `platformio.ini`, folder layout, secrets, and version header.

### Completed
- `platformio.ini` — board, framework, monitor speed, library deps, `build_type = debug`
- `include/version.h` — `FIRMWARE_VERSION "1.0.0"`, `FIRMWARE_MAJOR 1`, etc.
- `include/secrets_template.h` — Placeholders for credentials
- `src/main.cpp` — Minimal `setup()` / `loop()`
- Stub headers for each `lib/` module

---

## Phase 2: Display & UI ✅

**Goal**: AMOLED screen initialises and renders all screens.

### Completed
- `lib/display/display.h / display.cpp`
- `display_init()` — GPIO38 HIGH, AMOLED init with `beginAMOLED_191_SPI()`, LVGL init
- `display_tick()` — drives `lv_timer_handler()`
- Splash screen — Honey Yellow background, version subtitle
- Main screen — Teal weight card, Honey Yellow temp card, RSSI + battery status bar
- OTA progress screen — Dark Gray background, Teal progress bar
- Error screen — Alert Red background
- Calibration prompt, tare, weight entry, confirmation screens
- `display_set_status(const char *msg, bool success)` — updates status bar after upload (green/red)

### Notes
- Must use `beginAMOLED_191_SPI()` — the QSPI variant produces a blank screen on this board
- `_status_lbl` declared at file scope so `display_set_status` can access it after `display_show_main`

---

## Phase 3: Sensors ✅

**Goal**: Read real temperature and weight values from hardware.

### Completed
- `lib/sensors/sensors.h / sensors.cpp`
- DS18B20 temperature on GPIO 43 via `DallasTemperature` library
- HX711 weight on GPIO 44 (DOUT) / 45 (SCK), averages 5 readings
- `sensors_init()`, `sensors_read_temp_c()`, `sensors_read_weight_g()`
- `sensors_is_calibrated()` — checks `Preferences` for valid `cal_factor`
- `sensors_run_calibration_wizard()` — tare → reference weight → save to NVS

---

## Phase 4: Wi-Fi Provisioning ✅

**Goal**: First-boot SoftAP captive portal captures credentials; subsequent boots connect via STA.

### Completed
- `lib/provisioning/provisioning.h / provisioning.cpp`
- SoftAP SSID: `BeeConnect32-XXXX` (last 4 hex digits of MAC)
- Captive portal at `192.168.4.1` with DNS redirect
- Portal form: Wi-Fi SSID, Wi-Fi password, BEEP email, BEEP password, BEEP device key
- Logo served as PROGMEM binary at `GET /logo.png` (80×68 px, `Cache-Control: max-age=86400`)
- Credentials saved: `wifi/ssid`, `wifi/password`, `beep/email`, `beep/password`, `beep/device_id`
- `provisioning_connect_sta()` — 15 s timeout, shows error screen on failure
- `provisioning_reset()` — erases `wifi` namespace, triggered by `wifi_reset` serial command
- `provisioning_rssi_pct()` — returns RSSI as 0–100%

### Notes
- Base64-embedded logo in HTML caused connection resets (15 KB too large for WebServer.send())
- Fixed by serving logo as a separate `/logo.png` PROGMEM endpoint

---

## Phase 5: BEEP API Upload ✅

**Goal**: Sensor data and device metadata posted to BEEP.nl after each measurement cycle.

### Completed
- `lib/beep_api/beep_api.h / beep_api.cpp`
- `beep_login(email, password)` — POST `/api/login`, stores returned `api_token` as `beep/api_key`
- `beep_upload(temp_c, weight_g)` — POST `/api/sensors`
  - Payload: `{"t": <°C>, "weight_kg": <kg>, "rssi": <dBm>, "key": <device_key>}`
  - `weight_kg` omitted if NaN
  - Header: `Authorization: Bearer <api_key>`
- `WiFiClientSecure` created inline in each function (not returned by value) to avoid SSL context copy issues

### Notes
- Field name is `weight_kg` (not `weight`) — value in kg, not grams
- `rssi` field added from `WiFi.RSSI()`
- `key` field set to `beep/device_id` from NVS

---

## Phase 6: OTA Updates ✅

**Goal**: Pull-based OTA from GitHub releases on every wake.

### Completed
- `lib/ota/ota.h / ota.cpp`
- `ota_check_and_update()` — queries GitHub releases API, compares semver, downloads and flashes
- Queries `https://api.github.com/repos/dodichri/BeeConnect32/releases/latest`
- HTTP 404 treated as no release (returns `OTA_UP_TO_DATE`, not `OTA_FAILED`)
- Downloads `firmware.bin` asset via HTTPS, follows GitHub → S3 redirect
- Shows OTA progress screen during download
- Reboots on success
- `WiFiClientSecure` inlined in each function to avoid SSL EOF on copy

### Release Process
1. Bump `include/version.h`
2. Tag commit as `vMAJOR.MINOR.PATCH`
3. Attach `.pio/build/.../firmware.bin` as GitHub release asset named `firmware.bin`

---

## Phase 7: Diagnostics & Logging ✅

**Goal**: Serial console commands for field debugging; levelled logging.

### Completed
- `include/logger.h` — `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` macros
  - Active when `NDEBUG` is not defined (debug builds)
  - Compiled out in release builds
- `lib/diagnostics/diagnostics.h / diagnostics.cpp`
- `diagnostics_increment_boot_count()` — increments `diag/crash_cnt` in NVS
- `diagnostics_poll()` — parses serial input, dispatches commands
- `wifi_reset` command — calls `provisioning_reset()` and reboots
- `debug_metrics` command — prints free heap, uptime, crash count

---

## Phase 8: Integration & Deep Sleep ✅

**Goal**: All modules wired in `src/main.cpp`; deep sleep with sensor-only mode.

### Completed
- Full boot sequence in `setup()` (see §5 of FSD)
- Sensor-only mode:
  - Activated by short BOOT press during splash, or EXT0 wakeup from deep sleep
  - `run_sensor_only_mode()` — reads sensors every 15 s, updates display, never returns
  - No Wi-Fi, no upload, no sleep
- Deep sleep: `esp_deep_sleep(15ULL * 60ULL * 1000000ULL)`
- EXT0 wakeup: `esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0)` — BOOT button wakes into sensor-only mode
- Status bar updated after upload: green "Uploaded to BEEP" or red "BEEP upload failed"
- BEEP login runs on every boot (not just first boot) to keep token fresh

### Notes
- Wi-Fi connect moved inside `!sensor_only` block — prevents 15 s timeout and error screen in sensor-only mode
- Password kept in NVS (not deleted after login) to allow re-login on each wake cycle

---

## Phase 9: BEEP Device Update ✅

**Goal**: Register/update device metadata on BEEP platform on every wake.

### Completed
- `beep_update_device()` — POST `/api/devices/multiple`
- Payload (array of one device):
  ```json
  [{"key": "...", "hardware_id": "...", "firmware_version": "...",
    "boot_count": N, "measurement_interval_min": 15, "type": "Other", "delete": false}]
  ```
- Headers: `Content-Type`, `Accept`, `Origin: https://app.beep.nl`, `Referer: https://app.beep.nl/`, `User-Agent: Mozilla/5.0 (compatible; BeeConnect32)`, `Authorization: Bearer <api_key>`
- Called before sensor upload on each wake

### Notes
- `/api/devices/multiple` requires a fresh login token — the stored token from a previous boot was rejected (HTTP 401)
- Fix: re-login on every boot before calling device update
- PUT and PATCH endpoints returned 405/401; POST to `/api/devices/multiple` is correct
- `Origin` and `Referer` headers required to avoid 302 redirect to login page

---

## Phase 10: Battery Monitoring ✅

**Goal**: Read real battery voltage and state of charge from the onboard BQ25896 charge IC; include voltage in BEEP device update.

### Completed
- `lib/battery/battery.h / battery.cpp`
- `battery_voltage_mv()` — reads BQ25896 register 0x0E via I2C (address 0x6B)
  - Register formula: `bits[6:0] × 20 mV + 2304 mV`
  - Returns 0 when no battery connected (register reads `0x00` / 2304 mV base offset)
- `battery_pct()` — linear interpolation 3000–4200 mV → 0–100%
  - Returns 100% when no battery detected (USB powered)
- `display_show_main()` now receives real battery percentage instead of placeholder 100
- `beep_update_device(bat_mv)` — `battery_voltage` field (volts) added to device update payload; omitted when no battery connected

### Notes
- BQ25896 is the "unknown device at 0x6B" previously noted on the I2C bus
- `Arduino.h` must be included in `battery.cpp` before `logger.h` for `Serial` to be in scope
- No battery connected (USB-C only): register returns `0x00` → display shows 100%, `battery_voltage` omitted from BEEP payload

---

## Phase 11: Field Testing

### Checklist

- [ ] Splash screen displays correctly with version
- [ ] Weight and temperature match reference instruments (±1% weight, ±0.5°C)
- [ ] Calibration wizard saves and survives reboot
- [ ] BOOT button held during splash re-triggers calibration
- [ ] Short BOOT press during splash → sensor-only mode
- [ ] BOOT button press during deep sleep → sensor-only mode on wakeup
- [ ] Sensor-only mode shows live readings; reboot exits back to normal mode
- [ ] `wifi_reset` erases credentials and re-enters captive portal
- [ ] Captive portal logo displays correctly
- [ ] BEEP.nl dashboard shows sensor measurements at correct timestamps
- [ ] BEEP.nl device list shows correct firmware version, boot count, and battery voltage
- [ ] Battery percentage on screen matches expected state of charge when on battery
- [ ] Battery percentage shows 100% when on USB only
- [ ] OTA: tag a new release on GitHub; verify device picks it up on next wake
- [ ] `debug_metrics` output is correct
- [ ] Deep sleep current draw is within expected range for battery life estimates
- [ ] Device survives 24 h continuous operation (96 wake cycles)

---

## Dependency & Implementation Order

```
Phase 0 (Dev env) ✅
  └─ Phase 1 (Scaffold) ✅
       ├─ Phase 2 (Display) ✅
       ├─ Phase 3 (Sensors) ✅
       ├─ Phase 4 (Provisioning) ✅
       ├─ Phase 5 (BEEP API upload) ✅
       ├─ Phase 6 (OTA) ✅
       ├─ Phase 7 (Diagnostics + logging) ✅
       ├─ Phase 8 (Integration + deep sleep) ✅
       ├─ Phase 9 (BEEP device update) ✅
       ├─ Phase 10 (Battery monitoring) ✅
       └─ Phase 11 (Field testing)
```
