# BeeConnect32 — Implementation Plan

## Phase 0: Development Environment Setup

### 0.1 Prerequisites
- **OS**: Linux, macOS, or Windows with WSL2
- **Python**: 3.8+ (required by PlatformIO)
- **Git**
- **USB-C cable** capable of data transfer (not charge-only) for flashing the T-Display S3

### 0.2 Install PlatformIO Core (CLI)

```bash
# Install via pip (recommended for headless/CLI use)
pip install --user platformio

# Or install the VS Code extension "PlatformIO IDE" for GUI support
# Extension ID: platformio.platformio-ide
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

Add udev rules so PlatformIO can access the ESP32-S3 USB port without sudo:

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG dialout $USER   # log out and back in after this
```

### 0.4 Clone Repository and First Build

```bash
git clone https://github.com/dodichri/BeeConnect32.git
cd BeeConnect32

# Create your secrets file
cp include/secrets_template.h include/secrets.h
# Edit include/secrets.h and fill in:
#   WIFI_SSID, WIFI_PASSWORD, BEEP_API_KEY, OTA_AUTH_TOKEN

# First build (downloads toolchain + libraries automatically, takes 5–10 min)
pio run
```

### 0.5 Flash and Verify

Connect the T-Display S3 via USB-C, then:

```bash
pio run --target upload && pio device monitor
```

If the device is not detected, hold the BOOT button while plugging in USB to enter download mode.

---

## Phase 1: Project Scaffold

**Goal**: Compile-clean skeleton with correct `platformio.ini`, folder layout, secrets, and version header.

### Tasks

1. **`platformio.ini`** — Define board, framework, monitor speed, library deps, build flags.
   - Board: `lilygo-t-display-s3`
   - Framework: `arduino`
   - Monitor speed: `115200`
   - Partition scheme: supports OTA (use `default_8MB.csv` or `app3M_fat9M_16MB.csv`)
   - Library dependencies (see §4.2 of FSD)

2. **`include/version.h`** — `FIRMWARE_VERSION "1.0.0"`, `FIRMWARE_MAJOR 1`, etc.

3. **`include/secrets_template.h`** — Placeholders for all four credentials.

4. **`include/secrets.h`** — Add to `.gitignore`.

5. **`src/main.cpp`** — Minimal `setup()` and empty `loop()` that compiles.

6. **Stub headers** for each `lib/` module so `main.cpp` can `#include` them without errors.

**Exit criteria**: `pio run` completes without errors.

---

## Phase 2: Display & UI

**Goal**: AMOLED screen initialises and renders all five screens defined in §6.4 of the FSD.

**Dependency**: LilyGo AMOLED library + LVGL 8.x

### Tasks

1. **`lib/display/display.h / display.cpp`**
   - `display_init()` — GPIO38 HIGH, 1000 ms delay, AMOLED init, LVGL init.
   - `display_task()` — Call `lv_timer_handler()` periodically (or from main loop).

2. **Splash screen** (`display_show_splash(const char* version)`)
   - Honey Yellow (`#F5BB00`) background
   - Logo image from `docs/images/BeeConnect32_logo.png` (convert to C array with LVGL image converter)
   - "BeeConnect32" Montserrat 28 pt, Dark Gray
   - Firmware version Montserrat 14 pt, Dark Gray
   - 2 s duration

3. **Main screen** (`display_show_main(float weight_kg, float temp_c, int rssi_pct, int batt_pct)`)
   - Left card (Teal `#00A896`): weight in kg, Montserrat 28 pt
   - Right card (Honey Yellow): temperature °C, Montserrat 28 pt
   - Bottom status bar: Wi-Fi signal + battery %

4. **OTA progress screen** (`display_show_ota_progress(int percent)`)
   - Dark Gray background, Teal progress bar, percentage label

5. **Error screen** (`display_show_error(const char* msg)`)
   - Alert Red (`#E63946`) background, `!` icon, message text

6. **Calibration prompt screen** (`display_show_cal_prompt()`)
   - Shown when BOOT held during splash; "Hold 3 s to recalibrate" message

7. **Calibration wizard screens** (§6.5 of FSD)
   - Step 1: Tare prompt + Continue button
   - Step 2: Weight entry with `−` / `+` buttons (100 g steps, 100 g–50 kg) + Confirm
   - Step 3: Confirmation screen

**Exit criteria**: All screens render correctly on device, touch buttons register callbacks.

---

## Phase 3: Sensors

**Goal**: Read real temperature and weight values from hardware.

### Tasks

1. **`lib/sensors/sensors.h / sensors.cpp`**

2. **DS18B20 temperature** (`sensors_read_temp_c()`)
   - OneWire on GPIO 43, single device
   - `DallasTemperature` library
   - Return `float`; return `NAN` on error

3. **HX711 weight** — split into two concerns:

   a. **Raw driver** (`hx711_read_average(int samples)`)
      - Bit-bang on DOUT=GPIO44, SCK=GPIO45
      - Average `samples` readings (production uses 5)

   b. **Calibrated weight** (`sensors_read_weight_g()`)
      - Loads `cal_factor` + `tare` from `Preferences` namespace `hx711`
      - Returns `float` grams; returns `NAN` if uncalibrated

4. **Calibration logic** (`sensors_run_calibration_wizard()`)
   - Step 1: Average 10 raw readings → save as `tare`
   - Step 2: Read reference weight entered via touch UI → compute `cal_factor = (raw_avg - tare) / reference_g`
   - Step 3: Save to `Preferences`; show confirmation screen

5. **`sensors_is_calibrated()`** — Check `Preferences` for valid `cal_factor`.

**Exit criteria**: Serial log shows realistic temperature and weight values; calibration wizard saves and reloads correctly after reboot.

---

## Phase 4: Wi-Fi Provisioning

**Goal**: First-boot SoftAP captive portal captures credentials; subsequent boots connect via STA.

**Dependency**: `WebServer`, `Preferences`, `WiFi`, `DNSServer`

### Tasks

1. **`lib/provisioning/provisioning.h / provisioning.cpp`**

2. **`provisioning_has_credentials()`** — Check `Preferences` namespace `wifi` for `ssid`.

3. **SoftAP captive portal** (`provisioning_start_portal()`)
   - SSID: `BeeConnect32-XXXX` (last 4 hex digits of MAC)
   - IP: `192.168.4.1`
   - DNS server redirects all queries to `192.168.4.1` (captive portal redirect)
   - HTTP server serves a single-page form: SSID, password, BEEP API key
   - On submit: validate non-empty, save to `Preferences` (`wifi` + `beep` namespaces), reboot

4. **STA connect** (`provisioning_connect_sta()`)
   - Load SSID + password from `Preferences`
   - Attempt connection with 15 s timeout
   - On failure: show error screen; retry or re-enter portal after N failures

5. **`provisioning_reset()`** — Erase `wifi` namespace (triggered by `wifi_reset` serial command).

**Exit criteria**: Fresh device shows SoftAP; after credential entry device reboots and connects to Wi-Fi; `wifi_reset` restores portal flow.

---

## Phase 5: BEEP API Upload

**Goal**: Sensor data posted to BEEP.nl after each measurement cycle.

**Dependency**: `HTTPClient`, `ArduinoJson`, `WiFiClientSecure`

### Tasks

1. **`lib/beep_api/beep_api.h / beep_api.cpp`**

2. **`beep_api_upload(float temp_c, float weight_g)`**
   - Build JSON: `{"t": <temp_c>, "weight": <weight_g>}`
   - HTTPS POST to `https://api.beep.nl/api/sensors`
   - Header: `Authorization: Bearer <api_key>`
   - API key: read from `Preferences` namespace `beep` key `api_key`; fall back to compile-time `BEEP_API_KEY`
   - Return HTTP status code; log response body on non-2xx

3. **Root CA certificate** for `api.beep.nl` — embed as a `const char*` in the module (use `WiFiClientSecure::setCACert()`).

**Exit criteria**: Serial log shows HTTP 200 after upload; verify on BEEP.nl dashboard.

---

## Phase 6: OTA Updates

**Goal**: Authenticated OTA over HTTP while device is awake, with downgrade protection and rollback.

**Dependency**: `WebServer`, `Update`, `Preferences`

### Tasks

1. **`lib/ota/ota.h / ota.cpp`**

2. **`ota_server_start()`** — Start `WebServer` on port 3232.

3. **POST `/ota` handler**
   - Verify `Authorization: Bearer` matches `OTA_AUTH_TOKEN`; reject 401 if wrong
   - Parse `X-Firmware-Version` header: compare with `FIRMWARE_VERSION` using semver; reject 409 unless `?force=true`
   - Stream request body to `Update.write()`
   - If `X-Firmware-SHA256` present: verify after write; abort on mismatch
   - On success: save current partition label to `Preferences` namespace `ota` key `lkg_part`; call `ESP.restart()`
   - Send progress to display via `display_show_ota_progress()`

4. **Rollback guard** — On boot, if `Preferences` `ota/lkg_part` exists and differs from running partition, verify boot succeeded; revert if not (handled in `setup()` before other init).

**Exit criteria**: Flash new firmware via `curl`; device reboots into new version; downgrade rejected without `?force=true`.

Example OTA command:
```bash
curl -X POST http://<device-ip>:3232/ota \
  -H "Authorization: Bearer <OTA_AUTH_TOKEN>" \
  -H "X-Firmware-Version: 1.1.0" \
  --data-binary @.pio/build/lilygo-t-display-s3/firmware.bin
```

---

## Phase 7: Diagnostics

**Goal**: Serial console commands for field debugging.

### Tasks

1. **`lib/diagnostics/diagnostics.h / diagnostics.cpp`**

2. **`diagnostics_init()`** — Register serial command parser (called once in `setup()`).

3. **`diagnostics_tick()`** — Poll `Serial.available()` and dispatch commands (called in `loop()` or before deep sleep).

4. **`wifi_reset`** command — Calls `provisioning_reset()` and reboots.

5. **`debug_metrics`** command — Prints:
   - Free heap (`ESP.getFreeHeap()`)
   - Uptime (ms since boot)
   - Crash counter from `Preferences` namespace `diag` key `crash_cnt`

6. **Crash counter** — Increment in `setup()` before any other init; reset after a clean sleep cycle completes.

**Exit criteria**: Commands produce expected output in serial monitor.

---

## Phase 8: Integration & Boot Flow Wiring

**Goal**: All modules connected in `src/main.cpp` following the exact boot sequence from §5 of the FSD.

### Tasks

1. Wire `setup()` in order:
   ```
   Serial.begin(115200)
   diagnostics_init() + crash counter increment
   GPIO38 HIGH + delay(1000)
   display_init()
   display_show_splash(FIRMWARE_VERSION) + 2 s
   BOOT button check → display_show_cal_prompt() → calibration wizard
   provisioning_has_credentials() → portal or STA connect
   ota_server_start()
   sensors_is_calibrated() → calibration wizard if not
   sensors_read_temp_c() + sensors_read_weight_g()
   display_show_main(weight, temp, rssi, 100)
   beep_api_upload(temp, weight)
   diagnostics_tick()   // allow serial commands before sleep
   esp_deep_sleep(15ULL * 60ULL * 1000000ULL)
   ```

2. **`loop()`** — Only needed while awake for OTA and diagnostics:
   ```cpp
   ota_server_handle();      // WebServer.handleClient()
   diagnostics_tick();
   lv_timer_handler();
   ```

3. Verify **RSSI** is read after STA connect and passed to `display_show_main()`.

4. Verify **error screen** is shown on: Wi-Fi connect failure, sensor read error, BEEP API non-2xx.

**Exit criteria**: Full cycle (boot → splash → connect → read → upload → sleep) completes on real hardware with serial log matching boot flow.

---

## Phase 9: Hardware Validation & Field Testing

### Checklist

- [ ] Splash screen displays logo and version correctly
- [ ] Weight and temperature values match reference instruments (±1% weight, ±0.5°C)
- [ ] Calibration wizard saves and survives reboot
- [ ] BOOT button held during splash re-triggers calibration
- [ ] `wifi_reset` erases credentials and re-enters captive portal
- [ ] BEEP.nl dashboard shows uploaded measurements at correct timestamps
- [ ] OTA flash succeeds; downgrade rejected; rollback works on bad firmware
- [ ] `debug_metrics` output is correct
- [ ] Deep sleep current draw is within expected range for battery life estimates
- [ ] Device survives 24 h continuous operation (96 wake cycles)

---

## Dependency & Implementation Order Summary

```
Phase 0 (Dev env)
  └─ Phase 1 (Scaffold)
       ├─ Phase 2 (Display)     ← can start immediately after scaffold
       ├─ Phase 3 (Sensors)     ← can start in parallel with display
       ├─ Phase 4 (Provisioning)
       ├─ Phase 5 (BEEP API)    ← needs provisioning (Wi-Fi)
       ├─ Phase 6 (OTA)         ← needs display (progress screen)
       └─ Phase 7 (Diagnostics) ← standalone
            └─ Phase 8 (Integration)
                 └─ Phase 9 (Field testing)
```

Phases 2, 3, 4, and 7 can be developed in parallel once Phase 1 is complete. Phase 5 requires Phase 4 (Wi-Fi up). Phase 6 requires Phase 2 (progress screen). Phase 8 assembles everything.
