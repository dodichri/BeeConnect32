#pragma once

#include <LilyGo_AMOLED.h>

// Initialise display, LVGL, and touch. Must be called first in setup().
void display_init(void);

// Call from loop() or main logic to drive LVGL rendering.
void display_tick(void);

// ── Screens ──────────────────────────────────────────────────────────────────

// Honey Yellow splash for 2 s (blocking).
void display_show_splash(const char *version);

// Live sensor data screen.
void display_show_main(float weight_kg, float temp_c, int rssi_pct, int batt_pct);

// OTA progress (0–100).
void display_show_ota_progress(int percent);

// Red error screen with message.
void display_show_error(const char *msg);

// ── Calibration wizard ───────────────────────────────────────────────────────

// "Hold BOOT 3 s to recalibrate" prompt (non-blocking, returns immediately).
void display_show_cal_prompt(void);

// Step 1: tare. on_continue called when user taps Continue.
void display_show_cal_tare(void (*on_continue)(void));

// Step 2: weight entry (100 g steps, 100–50000 g). on_confirm called with chosen grams.
void display_show_cal_weight(void (*on_confirm)(float ref_g));

// Step 3: confirmation.
void display_show_cal_done(float ref_g);

// Update the status bar text after display_show_main() has been called.
// success=true shows text in green, false in red.
void display_set_status(const char *msg, bool success);

// Access the underlying board object (needed by sensors / OTA for sleep, etc.).
LilyGo_AMOLED &display_get_board(void);
