# BeeConnect32

**Beehive monitoring firmware for the LilyGo T-Display S3 AMOLED Plus (ESP32-S3)**

BeeConnect32 measures hive weight and internal temperature, displays live readings on a colour AMOLED screen, uploads data to the [BEEP.nl](https://beep.nl) beekeeping platform, and deep-sleeps for 15 minutes between cycles. A 6W solar panel and LiPo battery provide autonomous power in the field.

---

## Features

- **Weight monitoring** — HX711 load cell amplifier with touch-based calibration wizard
- **Temperature monitoring** — DS18B20 1-Wire sensor
- **AMOLED display** — Live sensor data, Wi-Fi signal, battery percentage
- **BEEP.nl integration** — Sensor upload, device registration, and firmware version tracking
- **Battery monitoring** — Real state-of-charge from BQ25896 charge IC via I2C
- **Solar charging** — BQ25896 accepts direct USB-C solar panel input with ICO (basic MPPT)
- **OTA updates** — Pull-based from GitHub releases; device checks for new firmware on every wake
- **Wi-Fi provisioning** — SoftAP captive portal on first boot; no app or serial setup required
- **Sensor-only mode** — Live readings without upload or sleep, activated via BOOT button
- **Deep sleep** — 15-minute sleep cycle for battery efficiency
- **Diagnostics** — Serial console commands and levelled logging (debug builds only)

---

## Hardware

| Component | Part |
|-----------|------|
| Board | LilyGo T-Display S3 AMOLED Plus (ESP32-S3R8) |
| Display | RM67162 AMOLED, 536×240 px |
| Temperature sensor | DS18B20 (1-Wire, GPIO 43) |
| Load cell ADC | HX711 (DOUT GPIO 44, SCK GPIO 45) |
| PMU | AXPM65611 |
| Charge IC | BQ25896 (USB-C input, JST-PH 1.25mm LiPo output) |
| Recommended battery | 3.7V LiPo, 2000–4000 mAh, JST-PH 1.25mm 2-pin |
| Solar panel | 6W, USB-C regulated output |

See [`docs/connection_diagram.md`](docs/connection_diagram.md) for full wiring details.

---

## Getting Started

### 1. Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI or VS Code extension)
- USB-C data cable
- Git

### 2. Clone and configure

```bash
git clone https://github.com/dodichri/BeeConnect32.git
cd BeeConnect32

cp include/secrets_template.h include/secrets.h
```

Edit `include/secrets.h` and fill in your credentials:

```cpp
#define WIFI_SSID        "your-network"
#define WIFI_PASSWORD    "your-password"
#define BEEP_API_KEY     ""           // optional compile-time fallback
#define OTA_AUTH_TOKEN   "your-token" // used to protect OTA endpoint
```

> `secrets.h` is git-ignored and never committed.

### 3. Build and flash

```bash
pio run --target upload
```

If the device is not detected, hold the BOOT button while plugging in USB to enter download mode.

### 4. First boot — Wi-Fi provisioning

On first boot the device creates a Wi-Fi access point:

- **SSID**: `BeeConnect32-XXXX` (last 4 hex digits of MAC)
- **Password**: none
- Connect your phone or laptop, then open `http://192.168.4.1`
- Enter your Wi-Fi credentials, BEEP email, BEEP password, and BEEP device key
- The device saves credentials and reboots automatically

---

## Boot Sequence

```
Power on / wake from sleep
  │
  ├─ Detect wakeup cause
  │    └─ BOOT button during sleep → sensor-only mode
  │
  ├─ Splash screen (2 s)
  │    ├─ Short BOOT press → sensor-only mode
  │    └─ Hold BOOT 3 s   → force scale recalibration
  │
  ├─ [Normal mode only]
  │    ├─ Wi-Fi connect
  │    ├─ BEEP login (refresh token)
  │    └─ OTA check (GitHub releases)
  │
  ├─ Sensor init + calibration check
  │
  ├─ [Sensor-only mode] ── Read → Display → repeat every 15 s (no sleep)
  │
  └─ [Normal mode]
       ├─ Read temperature + weight
       ├─ Update display
       ├─ BEEP device update (firmware version, boot count, battery voltage)
       ├─ BEEP sensor upload (temperature, weight, RSSI)
       ├─ Hold display 5 s
       └─ Deep sleep 15 minutes
```

---

## Scale Calibration

Calibration runs automatically on first boot (no saved calibration) or when BOOT is held for 3 seconds during the splash screen.

1. **Tare** — Remove all weight from the scale, tap Continue
2. **Reference weight** — Place a known weight, dial in its mass using the `−` / `+` buttons (100 g steps), tap Confirm
3. **Done** — Calibration factor saved to NVS; scale is ready

Calibration survives reboots and deep sleep.

---

## OTA Updates

The device checks GitHub releases on every wake. To publish a new release:

1. Bump `include/version.h`
2. Build: `pio run`
3. Tag the commit: `git tag v1.1.0 && git push --tags`
4. Create a GitHub release for the tag, attach `.pio/build/lilygo-t-display-s3-amoled-plus/firmware.bin` as `firmware.bin`

The device will download and flash the update on its next wake cycle.

---

## Serial Console

Connect at **115200 baud**. Available commands:

| Command | Description |
|---------|-------------|
| `wifi_reset` | Erase saved Wi-Fi credentials and reboot into captive portal |
| `debug_metrics` | Print free heap, uptime, and boot count |

Log output (`[INFO]`, `[WARN]`, `[ERROR]`) is active in debug builds and compiled out in release builds.

---

## NVS Storage

| Namespace | Keys | Contents |
|-----------|------|----------|
| `wifi` | `ssid`, `password` | Wi-Fi station credentials |
| `beep` | `api_key`, `email`, `password`, `device_id` | BEEP credentials and token |
| `hx711` | `cal_factor`, `tare` | Scale calibration |
| `diag` | `crash_cnt` | Boot counter |

---

## Project Structure

```
BeeConnect32/
  platformio.ini          # board config, framework, library deps
  include/
    version.h             # FIRMWARE_VERSION + MAJOR/MINOR/PATCH
    logger.h              # LOG_INFO / LOG_WARN / LOG_ERROR macros
    secrets_template.h    # copy to secrets.h and fill in credentials
  src/
    main.cpp              # boot sequence and main logic
  lib/
    battery/              # BQ25896 voltage and state-of-charge
    beep_api/             # BEEP login, sensor upload, device update
    diagnostics/          # boot counter, serial console commands
    display/              # LVGL UI — all screens and calibration wizard
    ota/                  # pull-based OTA from GitHub releases
    provisioning/         # SoftAP captive portal + STA connect
    sensors/              # DS18B20 temperature + HX711 weight
  docs/
    FSD/                  # Functional Specification Document
    connection_diagram.md # Sensor wiring diagrams
    implementation_plan.md
```

---

## Build Commands

```bash
pio run                                            # compile only
pio run --target upload                            # compile + flash
pio device monitor                                 # serial monitor (115200)
pio run --target upload && pio device monitor      # flash and monitor
pio run -e lilygo-t-display-s3-amoled-plus         # explicit environment
```

---

## License

MIT
