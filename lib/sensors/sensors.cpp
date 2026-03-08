#include "sensors.h"
#include "display.h"
#include "logger.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HX711.h>
#include <Preferences.h>
#include <math.h>

// ── Pin assignments ───────────────────────────────────────────────────────────
#define DS18B20_PIN  43
#define HX711_DOUT   44
#define HX711_SCK    45

// ── Peripherals ───────────────────────────────────────────────────────────────
static OneWire          _ow(DS18B20_PIN);
static DallasTemperature _dallas(&_ow);
static HX711            _scale;
static Preferences      _prefs;

// ── Init ──────────────────────────────────────────────────────────────────────

bool sensors_init(void)
{
    // DS18B20
    _dallas.begin();
    uint8_t count = _dallas.getDeviceCount();
    LOG_INFO("DS18B20 devices found: %d", count);

    // HX711
    _scale.begin(HX711_DOUT, HX711_SCK);
    if (!_scale.wait_ready_timeout(2000)) {
        LOG_ERROR("HX711 not ready");
        return false;
    }
    LOG_INFO("HX711 ready");

    // Restore calibration from NVS if available
    _prefs.begin("hx711", true);  // read-only
    if (_prefs.isKey("cal_factor")) {
        float cf   = _prefs.getFloat("cal_factor", 1.0f);
        long  tare = _prefs.getLong("tare", 0);
        _scale.set_scale(cf);
        _scale.set_offset(tare);
        LOG_INFO("Calibration loaded: factor=%.4f tare=%ld", cf, tare);
    }
    _prefs.end();

    return true;
}

// ── Calibration check ─────────────────────────────────────────────────────────

bool sensors_is_calibrated(void)
{
    _prefs.begin("hx711", true);
    bool ok = _prefs.isKey("cal_factor");
    _prefs.end();
    return ok;
}

// ── Temperature ───────────────────────────────────────────────────────────────

float sensors_read_temp_c(void)
{
    _dallas.requestTemperatures();
    float t = _dallas.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) {
        LOG_ERROR("DS18B20 read error");
        return NAN;
    }
    LOG_INFO("Temperature: %.2f C", t);
    return t;
}

// ── Weight ────────────────────────────────────────────────────────────────────

float sensors_read_weight_g(void)
{
    if (!sensors_is_calibrated()) {
        LOG_WARN("Scale not calibrated");
        return NAN;
    }
    if (!_scale.wait_ready_timeout(1000)) {
        LOG_ERROR("HX711 timeout");
        return NAN;
    }
    float g = _scale.get_units(5);
    LOG_INFO("Weight: %.1f g", g);
    return g;
}

// ── Calibration wizard ────────────────────────────────────────────────────────
// Blocking state machine driven by display touch callbacks.

static volatile enum { CAL_WAIT_TARE, CAL_DO_TARE, CAL_WAIT_WEIGHT, CAL_DO_WEIGHT, CAL_DONE }
    _cal_state;
static volatile float _cal_ref_g = 0.0f;

static void _on_tare_continue(void)   { _cal_state = CAL_DO_TARE; }
static void _on_weight_confirm(float g) { _cal_ref_g = g; _cal_state = CAL_DO_WEIGHT; }

void sensors_run_calibration_wizard(void)
{
    _cal_state = CAL_WAIT_TARE;
    display_show_cal_tare(_on_tare_continue);

    while (_cal_state != CAL_DONE) {
        display_tick();

        if (_cal_state == CAL_DO_TARE) {
            LOG_INFO("Taring...");
            _scale.set_scale();           // reset scale factor to 1
            _scale.tare(10);              // average 10 readings as zero

            long tare_offset = _scale.get_offset();
            LOG_INFO("Tare offset: %ld", tare_offset);

            _cal_state = CAL_WAIT_WEIGHT;
            display_show_cal_weight(_on_weight_confirm);
        }

        if (_cal_state == CAL_DO_WEIGHT) {
            float ref_g = _cal_ref_g;
            LOG_INFO("Calibrating with %.1f g reference", ref_g);

            // Raw value with reference weight on scale (scale factor still 1)
            float raw = _scale.get_value(10);
            float cal_factor = raw / ref_g;
            _scale.set_scale(cal_factor);

            LOG_INFO("cal_factor=%.4f  raw=%.1f", cal_factor, raw);

            // Persist to NVS
            _prefs.begin("hx711", false);
            _prefs.putFloat("cal_factor", cal_factor);
            _prefs.putLong("tare", _scale.get_offset());
            _prefs.end();

            _cal_state = CAL_DONE;
            display_show_cal_done(ref_g);   // blocking 2 s
        }
    }
}
