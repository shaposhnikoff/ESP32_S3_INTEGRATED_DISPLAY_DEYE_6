/* Internal UI interfaces shared by screens. All functions run in the LVGL
 * task (timers / event callbacks) and never block on the network. */
#pragma once

#include <stdbool.h>

#include "app_state.h"
#include "i18n.h"
#include "lvgl.h"

#define UI_W 1024
#define UI_H 600
#define UI_STATUS_H 56
#define UI_NAV_H 68
#define UI_CONTENT_W UI_W
#define UI_CONTENT_H (UI_H - UI_STATUS_H - UI_NAV_H)
#define UI_PAD 12

typedef struct {
    lv_color_t bg;
    lv_color_t card;
    lv_color_t border;
    lv_color_t text;
    lv_color_t muted;
    lv_color_t accent;
    lv_color_t ok;
    lv_color_t warn;
    lv_color_t err;
    lv_color_t pv;
    lv_color_t battery;
    lv_color_t grid;
    lv_color_t load;
    lv_color_t inactive;
} ui_palette_t;

extern ui_palette_t g_pal;
extern const lv_font_t deye_font_status;
extern const lv_font_t deye_font_inverter;
extern const lv_font_t deye_font_soc;
extern const lv_font_t deye_font_value;
extern const lv_font_t deye_font_tile;
extern const lv_font_t deye_font_data;

void ui_theme_init(lv_display_t *disp, bool dark);

/* ---- widgets ---- */
lv_obj_t *ui_card(lv_obj_t *parent, const char *title, int32_t w, int32_t h);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
/* Key/value row inside a card; returns the value label. */
lv_obj_t *ui_kv(lv_obj_t *parent, const char *key);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user);
void ui_set_enabled(lv_obj_t *obj, bool en);
void ui_flex_col(lv_obj_t *obj, int32_t gap);
void ui_flex_row(lv_obj_t *obj, int32_t gap);
lv_obj_t *ui_note(lv_obj_t *parent, const char *text); /* muted multi-line note */

/* Freshness-aware value: greys out text when data is not live. */
void ui_value_style(lv_obj_t *label, bool live);

/* Line chart bound to a history series. */
typedef struct {
    lv_obj_t *chart;
    lv_chart_series_t *ser;
    lv_obj_t *min_lbl;
    lv_obj_t *max_lbl;
    lv_obj_t *empty_lbl;
    uint16_t points;
} ui_chart_t;

void ui_chart_create(ui_chart_t *c, lv_obj_t *parent, int32_t w, int32_t h, uint16_t points, lv_color_t color);
/* Fills from history (try-lock; returns false if busy). */
bool ui_chart_fill(ui_chart_t *c, hist_range_t range, hist_series_t series, int64_t now_ms);
void ui_chart_set_color(ui_chart_t *c, lv_color_t color);

/* ---- dialogs ---- */
typedef void (*ui_confirm_cb_t)(bool confirmed, void *user);
void ui_confirm(const char *title, const char *text, const char *ok_text, ui_confirm_cb_t cb, void *user);
void ui_info(const char *title, const char *text);

/* Text entry with on-screen keyboard. `password` hides input. */
typedef void (*ui_text_cb_t)(const char *text, void *user);
void ui_text_input(const char *title, const char *initial, bool password, bool numeric, uint32_t max_len,
                   ui_text_cb_t cb, void *user);

/* ---- screens ---- */
typedef enum {
    SCR_OVERVIEW = 0,
    SCR_BATTERY,
    SCR_SOLAR,
    SCR_GRID,
    SCR_GENERATOR,
    SCR_CHARTS,
    SCR_CONTROLS,
    SCR_NETWORK,
    SCR_DIAG,
    SCR_SYSTEM,
    SCR_COUNT,
} ui_screen_id_t;

typedef struct {
    i18n_id_t title;
    const char *icon;
    bool protected_by_pin;
    void (*create)(lv_obj_t *parent);
    /* `changed` is true when new telemetry arrived since the last call. */
    void (*update)(const app_snapshot_t *s, bool changed);
} ui_screen_t;

extern const ui_screen_t scr_overview, scr_battery, scr_solar, scr_grid, scr_generator, scr_charts, scr_controls,
    scr_network, scr_diag, scr_system;

/* Helpers screens use for common decisions. */
bool ui_is_live(const app_snapshot_t *s);
/* Selects only monitoring tabs for LAN captures; caller holds the LVGL lock. */
bool ui_capture_select_screen(const char *name);
const char *ui_battery_state_text(im_value_t power_w);
/* Applies settings: persists, publishes, and triggers dependent actions. */
bool ui_save_settings(const app_settings_t *s, bool wifi_changed, bool backend_changed);
