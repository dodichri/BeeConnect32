#pragma once
#include <Arduino.h>

// Must be called once in setup() before any other sensors_ call.
bool sensors_init(void);

// True if a valid cal_factor exists in NVS.
bool sensors_is_calibrated(void);

// Read temperature from DS18B20. Returns NAN on error.
float sensors_read_temp_c(void);

// Read weight in grams using stored calibration. Returns NAN if uncalibrated or read error.
float sensors_read_weight_g(void);

// Blocking calibration wizard — drives the display wizard screens and waits
// for touch input. Call from setup() when calibration is needed.
void sensors_run_calibration_wizard(void);
