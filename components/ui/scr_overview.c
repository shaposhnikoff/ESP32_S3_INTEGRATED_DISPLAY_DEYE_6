/* Glanceable overview. All values come from the current telemetry snapshot. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fmt.h"
#include "ui_internal.h"

#define HEADER_Y 16
#define HEADER_H 120
#define CARD_Y 148
#define CARD_H 288
#define TILE_Y 448
#define TILE_H 72
#define LOW_SOC_PCT 25

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *value;
} overview_tile_t;

static lv_obj_t *s_header;
static lv_obj_t *s_header_badge;
static lv_obj_t *s_header_icon;
static lv_obj_t *s_headline;
static lv_obj_t *s_soc;
static lv_obj_t *s_percent;
static lv_obj_t *s_soc_track;
static lv_obj_t *s_soc_fill;
static lv_obj_t *s_inverter_status;
static lv_obj_t *s_inverter_badge;
static lv_obj_t *s_inverter_icon;
static lv_obj_t *s_load;
static lv_obj_t *s_data_dot;
static overview_tile_t s_tiles[6];

static lv_color_t color_green(void)
{
    return lv_color_hex(0x35D98A);
}
static lv_color_t color_yellow(void)
{
    return lv_color_hex(0xFFB020);
}
static lv_color_t color_red(void)
{
    return lv_color_hex(0xFF5A60);
}
static lv_color_t color_gray(void)
{
    return lv_color_hex(0xA0B0B9);
}

static lv_obj_t *label_at(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text, int32_t x,
                          int32_t y)
{
    lv_obj_t *label = ui_label(parent, font, color, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, g_pal.card, 0);
    lv_obj_set_style_border_color(obj, g_pal.border, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 17, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static overview_tile_t tile(lv_obj_t *page, int32_t x, int32_t w, const char *title)
{
    overview_tile_t t;
    t.card = panel(page, x, TILE_Y, w, TILE_H);
    lv_obj_set_style_border_width(t.card, 0, 0);
    t.title = label_at(t.card, &lv_font_montserrat_14, color_gray(), title, 12, 9);
    t.value = label_at(t.card, &deye_font_tile, g_pal.text, "--", 12, 31);
    return t;
}

static void create(lv_obj_t *page)
{
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    s_header = panel(page, 16, HEADER_Y, 992, HEADER_H);
    s_header_badge = panel(s_header, 0, 22, 72, 72);
    lv_obj_set_style_radius(s_header_badge, 36, 0);
    lv_obj_set_style_bg_opa(s_header_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_header_badge, 5, 0);
    s_header_icon = ui_label(s_header_badge, &lv_font_montserrat_48, color_green(), LV_SYMBOL_OK);
    lv_obj_center(s_header_icon);
    s_headline = label_at(s_header, &deye_font_status, lv_color_white(), "NO DATA", 180, 11);

    lv_obj_t *battery = panel(page, 16, CARD_Y, 490, CARD_H);
    label_at(battery, &lv_font_montserrat_24, color_gray(), LV_SYMBOL_BATTERY_FULL "  BATTERY", 24, 20);
    s_soc = label_at(battery, &deye_font_soc, color_green(), "--", 26, 70);
    s_percent = label_at(battery, &deye_font_inverter, color_green(), "%", 205, 133);
    s_soc_track = panel(battery, 24, 229, 438, 40);
    lv_obj_set_style_bg_color(s_soc_track, lv_color_hex(0x1C303B), 0);
    lv_obj_set_style_border_width(s_soc_track, 0, 0);
    lv_obj_set_style_radius(s_soc_track, 20, 0);
    s_soc_fill = panel(s_soc_track, 0, 0, 1, 40);
    lv_obj_set_style_bg_color(s_soc_fill, color_green(), 0);
    lv_obj_set_style_border_width(s_soc_fill, 0, 0);
    lv_obj_set_style_radius(s_soc_fill, 20, 0);
    for (int i = 1; i <= 3; i++) {
        lv_obj_t *tick = panel(s_soc_track, i * 109, 0, 4, 40);
        lv_obj_set_style_bg_color(tick, g_pal.card, 0);
        lv_obj_set_style_border_width(tick, 0, 0);
        lv_obj_set_style_radius(tick, 0, 0);
    }

    lv_obj_t *inverter = panel(page, 518, CARD_Y, 490, CARD_H);
    label_at(inverter, &lv_font_montserrat_24, color_gray(), LV_SYMBOL_CHARGE "  INVERTER", 24, 20);
    s_inverter_badge = panel(inverter, 428, 16, 34, 34);
    lv_obj_set_style_radius(s_inverter_badge, 17, 0);
    lv_obj_set_style_bg_opa(s_inverter_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_inverter_badge, color_green(), 0);
    lv_obj_set_style_border_width(s_inverter_badge, 3, 0);
    s_inverter_icon = ui_label(s_inverter_badge, &lv_font_montserrat_24, color_green(), LV_SYMBOL_OK);
    lv_obj_center(s_inverter_icon);
    s_inverter_status = label_at(inverter, &deye_font_inverter, color_green(), "N/A", 24, 85);
    lv_obj_t *rule = panel(inverter, 24, 206, 438, 1);
    lv_obj_set_style_bg_color(rule, g_pal.border, 0);
    lv_obj_set_style_border_width(rule, 0, 0);
    lv_obj_set_style_radius(rule, 0, 0);
    label_at(inverter, &lv_font_montserrat_24, color_gray(), "Load", 24, 232);
    s_load = ui_label(inverter, &deye_font_value, g_pal.text, "--");
    lv_obj_align(s_load, LV_ALIGN_TOP_RIGHT, -24, 218);

    s_tiles[0] = tile(page, 16, 169, "DATA");
    lv_obj_set_style_text_font(s_tiles[0].value, &deye_font_data, 0);
    s_data_dot = panel(s_tiles[0].card, 12, 38, 12, 12);
    lv_obj_set_style_radius(s_data_dot, 6, 0);
    lv_obj_set_style_border_width(s_data_dot, 0, 0);
    lv_obj_set_x(s_tiles[0].value, 32);
    s_tiles[1] = tile(page, 193, 182, "BATTERY");
    s_tiles[2] = tile(page, 383, 162, "RUNTIME*");
    s_tiles[3] = tile(page, 554, 138, "SOLAR");
    s_tiles[4] = tile(page, 700, 137, "GRID");
    s_tiles[5] = tile(page, 845, 163, "SIGNAL");
}

static void set_header(const char *text, const char *icon, lv_color_t color, lv_color_t bg, bool circle)
{
    lv_label_set_text(s_headline, text);
    const bool long_text = strlen(text) > 12;
    lv_obj_set_style_text_font(s_headline, long_text ? &deye_font_inverter : &deye_font_status, 0);
    lv_obj_set_y(s_headline, long_text ? 20 : 11);
    lv_obj_set_style_bg_color(s_header, bg, 0);
    lv_obj_set_style_border_color(s_header, color, 0);
    lv_obj_set_style_border_width(s_header, 3, 0);
    lv_obj_set_style_text_color(s_header_icon, color, 0);
    lv_obj_set_style_border_color(s_header_badge, color, 0);
    lv_obj_set_style_border_width(s_header_badge, circle ? 5 : 0, 0);
    lv_label_set_text(s_header_icon, icon);
    lv_obj_update_layout(s_headline);
    const int32_t width = lv_obj_get_width(s_headline);
    const int32_t x = (992 - (72 + 24 + width)) / 2;
    lv_obj_set_x(s_header_badge, x);
    lv_obj_set_x(s_headline, x + 96);
}

static const char *run_text(im_run_status_t status)
{
    switch (status) {
    case IM_RUN_NORMAL:
        return "NORMAL";
    case IM_RUN_FAULT:
        return "FAULT";
    case IM_RUN_ALARM:
        return "ALARM";
    case IM_RUN_STANDBY:
        return "STANDBY";
    case IM_RUN_SELFCHECK:
        return "CHECKING";
    default:
        return "N/A";
    }
}

static void set_power(lv_obj_t *label, im_value_t value, bool live, bool absolute)
{
    if (!live || !value.valid) {
        lv_label_set_text(label, "--");
        return;
    }
    char buf[32];
    fmt_power(buf, sizeof(buf), value, absolute);
    lv_label_set_text(label, buf);
}

static void update_header(const app_snapshot_t *s, bool live)
{
    const inverter_model_t *m = &s->model;
    if (!live) {
        set_header("NO DATA", "?", color_gray(), lv_color_hex(0x202B30), true);
    } else if (m->inverter.run_status == IM_RUN_FAULT) {
        set_header("INVERTER FAULT", LV_SYMBOL_CLOSE, color_red(), lv_color_hex(0x352024), true);
    } else if (m->inverter.run_status == IM_RUN_ALARM) {
        set_header("INVERTER ALARM", LV_SYMBOL_WARNING, color_yellow(), lv_color_hex(0x352D1A), false);
    } else if (m->battery.soc_pct.valid && m->battery.soc_pct.value <= LOW_SOC_PCT) {
        set_header("LOW BATTERY", LV_SYMBOL_WARNING, color_yellow(), lv_color_hex(0x352D1A), false);
    } else if (m->grid.connected.valid && m->grid.connected.value >= 0.5f) {
        set_header("ON GRID", LV_SYMBOL_OK, color_green(), lv_color_hex(0x102E26), true);
    } else if (m->grid.connected.valid && m->grid.connected.value < 0.5f && m->battery.power_w.valid &&
               m->battery.power_w.value > 20.0f) {
        set_header("ON BATTERY", LV_SYMBOL_OK, color_green(), lv_color_hex(0x102E26), true);
    } else if (m->grid.connected.valid && m->grid.connected.value < 0.5f) {
        set_header("GRID OFF", LV_SYMBOL_WARNING, color_yellow(), lv_color_hex(0x352D1A), false);
    } else {
        set_header("SYSTEM NORMAL", LV_SYMBOL_OK, color_green(), lv_color_hex(0x102E26), true);
    }
}

static void update_soc(const app_snapshot_t *s, bool live)
{
    const im_value_t soc = s->model.battery.soc_pct;
    const lv_color_t color = !live                      ? color_gray()
                             : !soc.valid               ? color_gray()
                             : soc.value <= 15          ? color_red()
                             : soc.value <= LOW_SOC_PCT ? color_yellow()
                                                        : color_green();
    if (soc.valid) {
        lv_label_set_text_fmt(s_soc, "%.0f", (double)soc.value);
    } else {
        lv_label_set_text(s_soc, "--");
    }
    lv_obj_set_style_text_color(s_soc, color, 0);
    lv_obj_set_style_text_color(s_percent, color, 0);
    lv_obj_update_layout(s_soc);
    const int32_t pct_x = 26 + lv_obj_get_width(s_soc) + 6;
    lv_obj_set_x(s_percent, pct_x);
    lv_obj_set_style_bg_color(s_soc_fill, color, 0);
    const int32_t fill = soc.valid ? (int32_t)(soc.value * 438 / 100) : 0;
    if (!soc.valid || fill <= 0) {
        lv_obj_add_flag(s_soc_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_soc_fill, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_width(s_soc_fill, fill > 0 ? fill : 1);
}

static void update_inverter(const app_snapshot_t *s, bool live)
{
    const im_run_status_t status = live ? s->model.inverter.run_status : IM_RUN_UNKNOWN;
    const lv_color_t color = !live                     ? color_gray()
                             : status == IM_RUN_NORMAL ? color_green()
                             : status == IM_RUN_FAULT  ? color_red()
                             : status == IM_RUN_ALARM  ? color_yellow()
                                                       : color_gray();
    lv_label_set_text(s_inverter_status, run_text(status));
    lv_obj_set_style_text_color(s_inverter_status, color, 0);
    lv_label_set_text(s_inverter_icon, status == IM_RUN_NORMAL  ? LV_SYMBOL_OK
                                       : status == IM_RUN_FAULT ? LV_SYMBOL_CLOSE
                                       : status == IM_RUN_ALARM ? LV_SYMBOL_WARNING
                                                                : "?");
    lv_obj_set_style_text_color(s_inverter_icon, color, 0);
    lv_obj_set_style_border_color(s_inverter_badge, color, 0);
    set_power(s_load, s->model.load.power_w, live, false);
}

static void update_tiles(const app_snapshot_t *s, bool live)
{
    char age[16];
    fmt_age_ms(age, sizeof(age), link_data_age_ms(&s->link, s->now_ms));
    const link_freshness_t freshness = link_freshness(&s->link, s->now_ms);
    const char *data = !s->backend_configured      ? "Setup"
                       : live                      ? "Live"
                       : freshness == LINK_OFFLINE ? "Lost"
                       : freshness == LINK_STALE   ? "Stale"
                                                   : "Waiting";
    lv_label_set_text_fmt(s_tiles[0].value, "%s%s%s", data, age[0] == '-' ? "" : " ", age[0] == '-' ? "" : age);
    lv_obj_set_style_bg_color(s_data_dot, live ? color_green() : color_red(), 0);
    lv_obj_set_style_text_color(s_tiles[0].value, g_pal.text, 0);

    const im_value_t bp = s->model.battery.power_w;
    const char *battery_state = !bp.valid        ? "BATTERY"
                                : bp.value > 20  ? "DISCHARGING"
                                : bp.value < -20 ? "CHARGING"
                                                 : "IDLE";
    lv_label_set_text(s_tiles[1].title, battery_state);
    set_power(s_tiles[1].value, bp, live, true);

    app_settings_t settings;
    app_settings_get(&settings);
    bool charging = false;
    const int32_t minutes = live ? im_battery_eta_minutes(s->model.battery.soc_pct, bp, settings.battery_capacity_wh,
                                                          settings.battery_cutoff_pct, &charging)
                                 : -1;
    lv_label_set_text(s_tiles[2].title, charging ? "FULL IN*" : "RUNTIME*");
    if (minutes < 0 || s->model.inverter.run_status == IM_RUN_FAULT) {
        lv_label_set_text(s_tiles[2].value, "--");
    } else if (minutes >= 60) {
        lv_label_set_text_fmt(s_tiles[2].value, "%ld h %ld m", (long)(minutes / 60), (long)(minutes % 60));
    } else {
        lv_label_set_text_fmt(s_tiles[2].value, "%ld m", (long)minutes);
    }

    set_power(s_tiles[3].value, s->model.pv.total_power_w, live, false);
    const im_value_t grid = s->model.grid.connected;
    lv_label_set_text(s_tiles[4].value, !live || !grid.valid ? "--" : grid.value >= 0.5f ? "ON" : "OFF");
    lv_obj_set_style_text_color(s_tiles[4].value,
                                !live || !grid.valid ? color_gray()
                                : grid.value >= 0.5f ? color_green()
                                : s->model.battery.soc_pct.valid && s->model.battery.soc_pct.value <= LOW_SOC_PCT
                                    ? color_yellow()
                                    : g_pal.text,
                                0);
    if (s->wifi.status == WIFI_ST_CONNECTED) {
        lv_label_set_text_fmt(s_tiles[5].value, "%d dBm", s->wifi.rssi);
    } else {
        lv_label_set_text(s_tiles[5].value, "--");
    }
    for (int i = 1; i < 6; i++) {
        if (i != 4) {
            lv_obj_set_style_text_color(s_tiles[i].value, live ? g_pal.text : color_gray(), 0);
        }
    }
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const bool live = ui_is_live(s);
    update_header(s, live);
    update_soc(s, live);
    update_inverter(s, live);
    update_tiles(s, live);
}

const ui_screen_t scr_overview = {T_OVERVIEW, LV_SYMBOL_HOME, false, create, update};
