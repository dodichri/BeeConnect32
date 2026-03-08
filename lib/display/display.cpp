#include "display.h"
#include "logger.h"
#include <LV_Helper.h>
#include <lvgl.h>
#include "logo.h"
#include "logo_sm.h"

// ── Board instance ────────────────────────────────────────────────────────────

static LilyGo_AMOLED _board;
static lv_obj_t      *_status_lbl = NULL;

LilyGo_AMOLED &display_get_board(void) { return _board; }

// ── Brand colours ─────────────────────────────────────────────────────────────

#define CLR_BG      lv_color_hex(0xF8F9FA)
#define CLR_YELLOW  lv_color_hex(0xF5BB00)
#define CLR_TEAL    lv_color_hex(0x00A896)
#define CLR_DARK    lv_color_hex(0x2D2D2D)
#define CLR_RED     lv_color_hex(0xE63946)
#define CLR_WHITE   lv_color_hex(0xFFFFFF)
#define CLR_GREEN   lv_color_hex(0x4CAF50)
#define CLR_GREY    lv_color_hex(0x888888)

// Screen dimensions
#define SCR_W 536
#define SCR_H 240

// Intermediate opacity values (LVGL only defines multiples of 10)
#define LV_OPA_8  20
#define LV_OPA_15 38
#define LV_OPA_18 46
#define LV_OPA_25 64
#define LV_OPA_28 71
#define LV_OPA_35 89
#define LV_OPA_45 115
#define LV_OPA_65 166
#define LV_OPA_75 191

// ── Internal helpers ──────────────────────────────────────────────────────────

static void clear_screen(lv_color_t bg)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
}

// Frameless, non-scrollable container
static lv_obj_t *make_panel(lv_obj_t *parent, lv_color_t bg,
                             int x, int y, int w, int h, int radius)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, bg, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, radius, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                             lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

// Outline circle (border only, transparent fill)
static lv_obj_t *make_circle(lv_obj_t *parent, lv_color_t border_color,
                              lv_opa_t border_opa, int x, int y, int d)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, d, d);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(c, border_color, 0);
    lv_obj_set_style_border_opa(c, border_opa, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

// ── Init ──────────────────────────────────────────────────────────────────────

void display_init(void)
{
    // T-Display S3 AMOLED Plus: CST816S (0x15) + BM8563 RTC (0x51) = SPI variant
    if (!_board.beginAMOLED_191_SPI()) {
        LOG_ERROR("beginAMOLED_191_SPI failed");
        return;
    }
    _board.setRotation(2);  // 180° — physical mounting orientation
    beginLvglHelper(_board);
}

// ── display_tick ─────────────────────────────────────────────────────────────

void display_tick(void)
{
    lv_task_handler();
}

// ── Splash ────────────────────────────────────────────────────────────────────

void display_show_splash(const char *version)
{
    clear_screen(CLR_YELLOW);
    lv_obj_t *scr = lv_scr_act();

    // Logo image — centred, upper half
    lv_obj_t *logo = lv_img_create(scr);
    lv_img_set_src(logo, &beeconnect32_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -52);

    lv_obj_t *title = make_label(scr, &lv_font_montserrat_32, CLR_DARK, "BeeConnect32");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 10);

    char ver_buf[32];
    snprintf(ver_buf, sizeof(ver_buf), "v %s", version);
    lv_obj_t *ver = make_label(scr, &lv_font_montserrat_16, CLR_DARK, ver_buf);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, 44);

    lv_obj_t *tag = make_label(scr, &lv_font_montserrat_14, CLR_DARK,
                                "Beehive Monitor - ESP32-S3");
    lv_obj_set_style_text_opa(tag, LV_OPA_70, 0);
    lv_obj_align(tag, LV_ALIGN_CENTER, 0, 66);

    // Boot progress dots (5 dots, row)
    const int DOT_D = 8, DOT_GAP = 7, DOT_Y = SCR_H - 20;
    const int row_w = 5 * DOT_D + 4 * DOT_GAP;
    int dot_x = (SCR_W - row_w) / 2;
    for (int i = 0; i < 5; i++) {
        lv_obj_t *dot = lv_obj_create(scr);
        lv_obj_set_pos(dot, dot_x, DOT_Y);
        lv_obj_set_size(dot, DOT_D, DOT_D);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        if (i < 2) {
            lv_obj_set_style_bg_color(dot, CLR_TEAL, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        } else if (i == 2) {
            lv_obj_set_style_bg_color(dot, CLR_DARK, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(dot, CLR_DARK, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_40, 0);
        }
        dot_x += DOT_D + DOT_GAP;
    }

    lv_task_handler();
    uint32_t splash_end = millis() + 2000;
    while (millis() < splash_end) { lv_task_handler(); delay(5); }
}

// ── Main screen ───────────────────────────────────────────────────────────────
// Layout: top-bar(36) | cards-area(176) | status-bar(28)

void display_show_main(float weight_kg, float temp_c, int rssi_pct, int batt_pct)
{
    clear_screen(CLR_BG);
    lv_obj_t *scr = lv_scr_act();

    // ── Top bar ──
    const int TB_H = 36;
    lv_obj_t *topbar = make_panel(scr, CLR_DARK, 0, 0, SCR_W, TB_H, 0);

    // Logo: 28×24 px pre-scaled image, centred vertically in the 36 px bar
    lv_obj_t *tb_logo = lv_img_create(topbar);
    lv_img_set_src(tb_logo, &beeconnect32_logo_sm);
    lv_obj_set_pos(tb_logo, 8, (TB_H - 24) / 2);

    // Title: starts 42 px from left (logo is visually ~26 px + 8 px margin)
    lv_obj_t *tb_title = make_label(topbar, &lv_font_montserrat_20, CLR_YELLOW, "BeeConnect32");
    lv_obj_set_pos(tb_title, 42, (TB_H - 20) / 2);

    char tb_right[32];
    snprintf(tb_right, sizeof(tb_right), "WiFi %d%%   Batt %d%%", rssi_pct, batt_pct);
    lv_obj_t *tb_info = make_label(topbar, &lv_font_montserrat_16, CLR_GREY, tb_right);
    lv_obj_align(tb_info, LV_ALIGN_RIGHT_MID, -14, 0);

    // ── Cards area ──
    const int CARDS_Y = TB_H;
    const int SB_H    = 28;
    const int CARDS_H = SCR_H - TB_H - SB_H;     // 176
    const int PAD     = 10;
    const int GAP     = 10;
    const int CARD_H  = CARDS_H - PAD * 2;        // 156
    const int CARD_W  = (SCR_W - PAD * 2 - GAP) / 2; // 253

    // Weight card — Teal
    lv_obj_t *wcard = make_panel(scr, CLR_TEAL,
                                 PAD, CARDS_Y + PAD, CARD_W, CARD_H, 12);
    lv_obj_set_style_pad_all(wcard, 14, 0);

    lv_obj_t *wlbl = make_label(wcard, &lv_font_montserrat_16, CLR_WHITE, "WEIGHT");
    lv_obj_align(wlbl, LV_ALIGN_TOP_LEFT, 0, 0);

    char wval_buf[16];
    if (isnan(weight_kg)) snprintf(wval_buf, sizeof(wval_buf), "---");
    else                  snprintf(wval_buf, sizeof(wval_buf), "%.2f", weight_kg);
    lv_obj_t *wval = make_label(wcard, &lv_font_montserrat_36, CLR_WHITE, wval_buf);
    lv_obj_align(wval, LV_ALIGN_LEFT_MID, 0, -6);

    lv_obj_t *wunit = make_label(wcard, &lv_font_montserrat_16, CLR_WHITE, "kilograms");
    lv_obj_align(wunit, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Temperature card — Honey Yellow
    lv_obj_t *tcard = make_panel(scr, CLR_YELLOW,
                                 PAD + CARD_W + GAP, CARDS_Y + PAD, CARD_W, CARD_H, 12);
    lv_obj_set_style_pad_all(tcard, 14, 0);

    lv_obj_t *tlbl = make_label(tcard, &lv_font_montserrat_16, CLR_DARK, "TEMPERATURE");
    lv_obj_align(tlbl, LV_ALIGN_TOP_LEFT, 0, 0);

    char tval_buf[16];
    if (isnan(temp_c)) snprintf(tval_buf, sizeof(tval_buf), "---");
    else               snprintf(tval_buf, sizeof(tval_buf), "%.1f\xc2\xb0", temp_c);  // °
    lv_obj_t *tval = make_label(tcard, &lv_font_montserrat_36, CLR_DARK, tval_buf);
    lv_obj_align(tval, LV_ALIGN_LEFT_MID, 0, -6);

    lv_obj_t *tunit = make_label(tcard, &lv_font_montserrat_16, CLR_DARK, "Celsius");
    lv_obj_align(tunit, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // ── Status bar ──
    lv_obj_t *sbar = make_panel(scr, CLR_DARK, 0, SCR_H - SB_H, SCR_W, SB_H, 0);
    lv_obj_set_style_pad_left(sbar, 14, 0);
    lv_obj_set_style_pad_right(sbar, 14, 0);

    _status_lbl = make_label(sbar, &lv_font_montserrat_16, CLR_GREY,
                             "Uploading to BEEP...");
    lv_obj_align(_status_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *online = make_label(sbar, &lv_font_montserrat_16, CLR_GREEN, "ONLINE");
    lv_obj_align(online, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_task_handler();
}

void display_set_status(const char *msg, bool success)
{
    if (!_status_lbl) return;
    lv_label_set_text(_status_lbl, msg);
    lv_obj_set_style_text_color(_status_lbl,
        success ? CLR_GREEN : CLR_RED, 0);
    lv_task_handler();
}

// ── OTA progress ──────────────────────────────────────────────────────────────

static lv_obj_t *_ota_fill = NULL;
static lv_obj_t *_ota_pct  = NULL;

void display_show_ota_progress(int percent)
{
    if (_ota_fill == NULL) {
        clear_screen(CLR_DARK);
        lv_obj_t *scr = lv_scr_act();

        lv_obj_t *title = make_label(scr, &lv_font_montserrat_20, CLR_WHITE, "OTA Update");
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

        lv_obj_t *sub = make_label(scr, &lv_font_montserrat_14, CLR_WHITE,
                                   "Flashing firmware - do not power off");
        lv_obj_set_style_text_opa(sub, LV_OPA_COVER, 0);
        lv_obj_align(sub, LV_ALIGN_CENTER, 0, -24);

        // Slim track (10px height)
        const int BAR_W = 400, BAR_H = 10;
        lv_obj_t *track = make_panel(scr, lv_color_hex(0x444444),
                                     (SCR_W - BAR_W) / 2, SCR_H / 2 - BAR_H / 2,
                                     BAR_W, BAR_H, 6);

        // Teal fill (starts at 0 width)
        _ota_fill = make_panel(track, CLR_TEAL, 0, 0, 0, BAR_H, 6);

        // Percentage label
        _ota_pct = make_label(scr, &lv_font_montserrat_16, CLR_TEAL, "0%");
        lv_obj_align(_ota_pct, LV_ALIGN_CENTER, 0, 26);
    }

    // Update fill width proportionally
    lv_obj_t *track = lv_obj_get_parent(_ota_fill);
    int track_w = lv_obj_get_width(track);
    lv_obj_set_width(_ota_fill, track_w * percent / 100);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(_ota_pct, buf);

    lv_task_handler();
}

// ── Error screen ─────────────────────────────────────────────────────────────

void display_show_error(const char *msg)
{
    _ota_fill = NULL;
    _ota_pct  = NULL;

    clear_screen(CLR_RED);
    lv_obj_t *scr = lv_scr_act();

    // Circle with ✕
    const int CIR_D = 56;
    lv_obj_t *circle = make_circle(scr, CLR_WHITE, LV_OPA_35,
                                   (SCR_W - CIR_D) / 2, SCR_H / 2 - 70, CIR_D);
    lv_obj_set_style_bg_color(circle, CLR_WHITE, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_15, 0);

    lv_obj_t *x_lbl = make_label(circle, &lv_font_montserrat_28, CLR_WHITE, "X");
    lv_obj_center(x_lbl);

    lv_obj_t *title = make_label(scr, &lv_font_montserrat_20, CLR_WHITE, "Error");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *mlbl = make_label(scr, &lv_font_montserrat_14, CLR_WHITE, msg);
    lv_obj_set_style_text_opa(mlbl, LV_OPA_65, 0);
    lv_obj_set_style_text_align(mlbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(mlbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(mlbl, 460);
    lv_obj_align(mlbl, LV_ALIGN_CENTER, 0, 22);

    // Footer pill
    const int PILL_W = 260, PILL_H = 26;
    lv_obj_t *pill = make_panel(scr, lv_color_hex(0x000000),
                                (SCR_W - PILL_W) / 2, SCR_H - 46, PILL_W, PILL_H, 13);
    lv_obj_set_style_bg_opa(pill, LV_OPA_20, 0);

    lv_obj_t *pill_lbl = make_label(pill, &lv_font_montserrat_14, CLR_WHITE,
                                    "Retrying next wake in  15:00");
    lv_obj_set_style_text_opa(pill_lbl, LV_OPA_75, 0);
    lv_obj_center(pill_lbl);

    lv_task_handler();
}

// ── Calibration wizard — shared style ────────────────────────────────────────

// All wizard screens use a dark background with a step label, icon circle,
// title, body text, and a teal pill button.

static void wiz_base(lv_obj_t **scr_out, const char *step_str)
{
    clear_screen(CLR_DARK);
    lv_obj_t *scr = lv_scr_act();
    *scr_out = scr;

    // Step label — top right
    lv_obj_t *step = make_label(scr, &lv_font_montserrat_16, CLR_WHITE, step_str);
    lv_obj_set_style_text_opa(step, LV_OPA_28, 0);
    lv_obj_align(step, LV_ALIGN_TOP_RIGHT, -16, 10);
}

static lv_obj_t *wiz_icon_circle(lv_obj_t *scr, const char *symbol, int cy_offset)
{
    const int D = 52;
    int cx = (SCR_W - D) / 2;
    int cy = SCR_H / 2 + cy_offset - D / 2;

    lv_obj_t *circle = make_panel(scr, CLR_TEAL, cx, cy, D, D, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(circle, LV_OPA_15, 0);
    lv_obj_set_style_border_color(circle, CLR_TEAL, 0);
    lv_obj_set_style_border_opa(circle, LV_OPA_40, 0);
    lv_obj_set_style_border_width(circle, 2, 0);

    lv_obj_t *sym = make_label(circle, &lv_font_montserrat_20, CLR_TEAL, symbol);
    lv_obj_center(sym);

    return circle;
}

static lv_obj_t *wiz_pill_btn(lv_obj_t *scr, const char *text, lv_color_t bg, lv_color_t fg)
{
    const int BW = 180, BH = 48;
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, BW, BH);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, BH / 2, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *lbl = make_label(btn, &lv_font_montserrat_20, fg, text);
    lv_obj_center(lbl);
    return btn;
}

// ── Cal prompt ────────────────────────────────────────────────────────────────

void display_show_cal_prompt(void)
{
    lv_obj_t *scr;
    wiz_base(&scr, "");

    lv_obj_t *lbl = make_label(scr, &lv_font_montserrat_28, CLR_YELLOW,
                                "Hold BOOT 3 s\nto recalibrate");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_task_handler();
}

// ── Cal step 1 — Tare ─────────────────────────────────────────────────────────

static void (*_tare_cb)(void) = NULL;
static void _tare_btn_cb(lv_event_t *e) { if (_tare_cb) _tare_cb(); }

void display_show_cal_tare(void (*on_continue)(void))
{
    _tare_cb = on_continue;

    lv_obj_t *scr;
    wiz_base(&scr, "1 / 3");

    wiz_icon_circle(scr, "O", -60);

    lv_obj_t *title = make_label(scr, &lv_font_montserrat_28, CLR_WHITE, "Tare Scale");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *body = make_label(scr, &lv_font_montserrat_16, CLR_WHITE,
                                "Remove all weight from the scale\nthen tap Continue.");
    lv_obj_set_style_text_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 26);

    lv_obj_t *btn = wiz_pill_btn(scr, "Continue", CLR_TEAL, CLR_WHITE);
    lv_obj_add_event_cb(btn, _tare_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_task_handler();
}

// ── Cal step 2 — Weight entry ─────────────────────────────────────────────────

static void (*_weight_cb)(float) = NULL;
static float _ref_g = 2000.0f;
static lv_obj_t *_weight_val_lbl  = NULL;
static lv_obj_t *_weight_unit_lbl = NULL;

static void _update_weight_display(void)
{
    if (!_weight_val_lbl) return;
    char vbuf[12], ubuf[12];
    if (_ref_g >= 1000.0f) {
        snprintf(vbuf, sizeof(vbuf), "%.1f", _ref_g / 1000.0f);
        snprintf(ubuf, sizeof(ubuf), "kilograms");
    } else {
        snprintf(vbuf, sizeof(vbuf), "%.0f", _ref_g);
        snprintf(ubuf, sizeof(ubuf), "grams");
    }
    lv_label_set_text(_weight_val_lbl, vbuf);
    lv_label_set_text(_weight_unit_lbl, ubuf);
}

static void _minus_cb(lv_event_t *e)
{
    if (_ref_g > 100.0f) { _ref_g -= 100.0f; _update_weight_display(); }
}
static void _plus_cb(lv_event_t *e)
{
    if (_ref_g < 50000.0f) { _ref_g += 100.0f; _update_weight_display(); }
}
static void _confirm_weight_cb(lv_event_t *e)
{
    if (_weight_cb) _weight_cb(_ref_g);
}

void display_show_cal_weight(void (*on_confirm)(float ref_g))
{
    _weight_cb = on_confirm;
    _ref_g = 2000.0f;

    lv_obj_t *scr;
    wiz_base(&scr, "2 / 3");

    lv_obj_t *title = make_label(scr, &lv_font_montserrat_28, CLR_WHITE,
                                 "Place Reference Weight");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *body = make_label(scr, &lv_font_montserrat_16, CLR_WHITE,
                                "Put a known weight on the scale,\nthen dial in its mass below.");
    lv_obj_set_style_text_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 48);

    // Selector row
    // Layout (screen h=240, body ends ~y=92, confirm btn top ~y=176):
    //   value centre  y=120+10=130  (36pt: top=112, bot=148)
    //   buttons       y=120+10=130  (48px: top=106, bot=154)  x=±100
    //   unit          y=120+10+26=156 (16pt: top=146, bot=166) → gap to btn=10
    const int ADJ_D = 48;
    const int SEL_Y = 10;

    // Minus button (circular)
    lv_obj_t *btn_m = lv_btn_create(scr);
    lv_obj_set_size(btn_m, ADJ_D, ADJ_D);
    lv_obj_align(btn_m, LV_ALIGN_CENTER, -100, SEL_Y);
    lv_obj_set_style_bg_color(btn_m, CLR_WHITE, 0);
    lv_obj_set_style_bg_opa(btn_m, LV_OPA_15, 0);
    lv_obj_set_style_border_color(btn_m, CLR_WHITE, 0);
    lv_obj_set_style_border_opa(btn_m, LV_OPA_40, 0);
    lv_obj_set_style_border_width(btn_m, 2, 0);
    lv_obj_set_style_radius(btn_m, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(btn_m, 0, 0);
    lv_obj_add_event_cb(btn_m, _minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lm = make_label(btn_m, &lv_font_montserrat_28, CLR_WHITE, "-");
    lv_obj_center(lm);

    // Value + unit
    _weight_val_lbl = make_label(scr, &lv_font_montserrat_36, CLR_YELLOW, "2.0");
    lv_obj_align(_weight_val_lbl, LV_ALIGN_CENTER, 0, SEL_Y);
    _weight_unit_lbl = make_label(scr, &lv_font_montserrat_16, CLR_WHITE, "kilograms");
    lv_obj_set_style_text_opa(_weight_unit_lbl, LV_OPA_COVER, 0);
    lv_obj_align(_weight_unit_lbl, LV_ALIGN_CENTER, 0, SEL_Y + 26);

    // Plus button (circular)
    lv_obj_t *btn_p = lv_btn_create(scr);
    lv_obj_set_size(btn_p, ADJ_D, ADJ_D);
    lv_obj_align(btn_p, LV_ALIGN_CENTER, 100, SEL_Y);
    lv_obj_set_style_bg_color(btn_p, CLR_WHITE, 0);
    lv_obj_set_style_bg_opa(btn_p, LV_OPA_15, 0);
    lv_obj_set_style_border_color(btn_p, CLR_WHITE, 0);
    lv_obj_set_style_border_opa(btn_p, LV_OPA_40, 0);
    lv_obj_set_style_border_width(btn_p, 2, 0);
    lv_obj_set_style_radius(btn_p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(btn_p, 0, 0);
    lv_obj_add_event_cb(btn_p, _plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = make_label(btn_p, &lv_font_montserrat_28, CLR_WHITE, "+");
    lv_obj_center(lp);

    lv_obj_t *btn = wiz_pill_btn(scr, "Confirm", CLR_TEAL, CLR_WHITE);
    lv_obj_add_event_cb(btn, _confirm_weight_cb, LV_EVENT_CLICKED, NULL);

    lv_task_handler();
}

// ── Cal step 3 — Done ────────────────────────────────────────────────────────

void display_show_cal_done(float ref_g)
{
    _weight_val_lbl  = NULL;
    _weight_unit_lbl = NULL;

    lv_obj_t *scr;
    wiz_base(&scr, "3 / 3");

    // Done screen vertical layout (h=240, btn top=176):
    //   circle  y=10  h=52  → bot=62
    //   title   y=68  h=28  → bot=96
    //   chip    y=104 h=32  → bot=136
    //   body    y=144 h=20  → bot=164  gap to btn=12
    const int D = 52;
    lv_obj_t *circle = make_panel(scr, CLR_TEAL,
                                  (SCR_W - D) / 2, 10, D, D,
                                  LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(circle, LV_OPA_18, 0);
    lv_obj_set_style_border_color(circle, CLR_TEAL, 0);
    lv_obj_set_style_border_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_t *check = make_label(circle, &lv_font_montserrat_20, CLR_TEAL, "OK");
    lv_obj_center(check);

    lv_obj_t *title = make_label(scr, &lv_font_montserrat_28, CLR_WHITE, "Calibration Saved");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 68);

    // Reference chip
    char chip_buf[32];
    if (ref_g >= 1000.0f)
        snprintf(chip_buf, sizeof(chip_buf), "Reference: %.1f kg", ref_g / 1000.0f);
    else
        snprintf(chip_buf, sizeof(chip_buf), "Reference: %.0f g", ref_g);

    const int CHIP_W = 240, CHIP_H = 32;
    lv_obj_t *chip = make_panel(scr, CLR_TEAL,
                                (SCR_W - CHIP_W) / 2, 104,
                                CHIP_W, CHIP_H, 16);
    lv_obj_set_style_bg_opa(chip, LV_OPA_15, 0);
    lv_obj_set_style_border_color(chip, CLR_TEAL, 0);
    lv_obj_set_style_border_opa(chip, LV_OPA_30, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_t *chip_lbl = make_label(chip, &lv_font_montserrat_16, CLR_TEAL, chip_buf);
    lv_obj_center(chip_lbl);

    lv_obj_t *body = make_label(scr, &lv_font_montserrat_16, CLR_WHITE,
                                "Factor stored to NVS. Scale is ready.");
    lv_obj_set_style_text_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 144);

    wiz_pill_btn(scr, "Done", CLR_YELLOW, CLR_DARK);

    lv_task_handler();
    uint32_t done_end = millis() + 2000;
    while (millis() < done_end) { lv_task_handler(); delay(5); }
}
