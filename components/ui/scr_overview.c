/* Overview: energy-flow diagram plus summary cards. */
#include <math.h>
#include <stdio.h>

#include "fmt.h"
#include "ui_internal.h"

#define FLOW_W 520
#define FLOW_H 520
#define NODE_W 150
#define NODE_H 92
#define MIN_FLOW_W 5.0f /* below this a path is treated as idle */

typedef enum { DIR_NONE = 0, DIR_TO_INV, DIR_FROM_INV, DIR_UNKNOWN } flow_dir_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *value;
    lv_obj_t *sub;
    lv_obj_t *line;
    lv_obj_t *arrow;
    lv_point_precise_t pts[2];
    lv_color_t color;
    const char *sym_to_inv;   /* arrow glyph when flowing towards the inverter */
    const char *sym_from_inv; /* arrow glyph when flowing away from it */
} flow_node_t;

enum { N_PV = 0, N_GRID, N_LOAD, N_BAT, N_COUNT };

static flow_node_t s_nodes[N_COUNT];
static lv_obj_t *s_inv_status;
static lv_obj_t *s_soc_bar;
static lv_obj_t *s_soc_lbl;
static lv_obj_t *s_bat_state;
static lv_obj_t *s_mode_lbl;
static lv_obj_t *s_gen_lbl;
static lv_obj_t *s_today[6];
static lv_obj_t *s_comm_lbl;

static lv_obj_t *make_node(lv_obj_t *parent, flow_node_t *n, const char *icon, const char *name, int32_t x, int32_t y,
                           lv_color_t color)
{
    n->color = color;
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, NODE_W, NODE_H);
    lv_obj_set_pos(b, x - NODE_W / 2, y - NODE_H / 2);
    lv_obj_set_style_bg_color(b, g_pal.bg, 0);
    lv_obj_set_style_border_color(b, color, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_pad_all(b, 6, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    ui_flex_col(b, 0);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *t = ui_label(b, &lv_font_montserrat_14, color, "");
    lv_label_set_text_fmt(t, "%s %s", icon, name);
    n->value = ui_label(b, &lv_font_montserrat_24, g_pal.text, "--");
    n->sub = ui_label(b, &lv_font_montserrat_14, g_pal.muted, "");
    n->box = b;
    return b;
}

static void make_link(lv_obj_t *parent, flow_node_t *n, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    n->pts[0].x = x1;
    n->pts[0].y = y1;
    n->pts[1].x = x2;
    n->pts[1].y = y2;
    n->line = lv_line_create(parent);
    lv_line_set_points(n->line, n->pts, 2);
    lv_obj_set_style_line_rounded(n->line, true, 0);
    n->arrow = ui_label(parent, &lv_font_montserrat_28, n->color, "");
    lv_obj_set_pos(n->arrow, (x1 + x2) / 2 - 12, (y1 + y2) / 2 - 16);
}

static void set_link(flow_node_t *n, flow_dir_t dir, float watts, float rated)
{
    const bool active = dir == DIR_TO_INV || dir == DIR_FROM_INV || dir == DIR_UNKNOWN;
    float frac = rated > 0 ? fabsf(watts) / rated : 0;
    if (frac > 1) {
        frac = 1;
    }
    lv_obj_set_style_line_width(n->line, active ? (int32_t)(3 + 9 * frac) : 2, 0);
    lv_obj_set_style_line_color(n->line, active ? n->color : g_pal.inactive, 0);
    lv_obj_set_style_line_dash_width(n->line, active ? 0 : 6, 0);
    lv_obj_set_style_line_dash_gap(n->line, active ? 0 : 6, 0);
    switch (dir) {
    case DIR_TO_INV:
        lv_label_set_text(n->arrow, n->sym_to_inv);
        break;
    case DIR_FROM_INV:
        lv_label_set_text(n->arrow, n->sym_from_inv);
        break;
    case DIR_UNKNOWN:
        lv_label_set_text(n->arrow, "?");
        break;
    default:
        lv_label_set_text(n->arrow, "");
        break;
    }
}

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *flow = ui_card(page, NULL, FLOW_W, FLOW_H);
    lv_obj_set_layout(flow, LV_LAYOUT_NONE);
    const int32_t cx = (FLOW_W - 2 * UI_PAD) / 2;
    const int32_t cy = (FLOW_H - 2 * UI_PAD) / 2;
    const int32_t dx = 175;
    const int32_t dy = 185;

    /* links first so nodes draw on top */
    s_nodes[N_PV].sym_to_inv = LV_SYMBOL_DOWN;
    s_nodes[N_PV].sym_from_inv = LV_SYMBOL_UP;
    s_nodes[N_BAT].sym_to_inv = LV_SYMBOL_UP;
    s_nodes[N_BAT].sym_from_inv = LV_SYMBOL_DOWN;
    s_nodes[N_GRID].sym_to_inv = LV_SYMBOL_RIGHT;
    s_nodes[N_GRID].sym_from_inv = LV_SYMBOL_LEFT;
    s_nodes[N_LOAD].sym_to_inv = LV_SYMBOL_LEFT;
    s_nodes[N_LOAD].sym_from_inv = LV_SYMBOL_RIGHT;
    s_nodes[N_PV].color = g_pal.pv;
    s_nodes[N_GRID].color = g_pal.grid;
    s_nodes[N_LOAD].color = g_pal.load;
    s_nodes[N_BAT].color = g_pal.battery;
    make_link(flow, &s_nodes[N_PV], cx, cy - dy, cx, cy);
    make_link(flow, &s_nodes[N_BAT], cx, cy + dy, cx, cy);
    make_link(flow, &s_nodes[N_GRID], cx - dx, cy, cx, cy);
    make_link(flow, &s_nodes[N_LOAD], cx + dx, cy, cx, cy);

    make_node(flow, &s_nodes[N_PV], LV_SYMBOL_IMAGE, "SOLAR", cx, cy - dy, g_pal.pv);
    make_node(flow, &s_nodes[N_BAT], LV_SYMBOL_BATTERY_FULL, "BATTERY", cx, cy + dy, g_pal.battery);
    make_node(flow, &s_nodes[N_GRID], LV_SYMBOL_POWER, "GRID", cx - dx, cy, g_pal.grid);
    make_node(flow, &s_nodes[N_LOAD], LV_SYMBOL_HOME, "HOUSE", cx + dx, cy, g_pal.load);

    lv_obj_t *inv = lv_obj_create(flow);
    lv_obj_set_size(inv, 120, 84);
    lv_obj_set_pos(inv, cx - 60, cy - 42);
    lv_obj_set_style_bg_color(inv, g_pal.card, 0);
    lv_obj_set_style_border_color(inv, g_pal.accent, 0);
    lv_obj_set_style_border_width(inv, 3, 0);
    lv_obj_set_style_radius(inv, 14, 0);
    lv_obj_remove_flag(inv, LV_OBJ_FLAG_SCROLLABLE);
    ui_flex_col(inv, 0);
    lv_obj_set_flex_align(inv, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_label(inv, &lv_font_montserrat_16, g_pal.accent, LV_SYMBOL_CHARGE " INVERTER");
    s_inv_status = ui_label(inv, &lv_font_montserrat_14, g_pal.text, "--");

    lv_obj_t *col = lv_obj_create(page);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, UI_CONTENT_W - FLOW_W - 3 * UI_PAD, FLOW_H);
    ui_flex_col(col, UI_PAD);
    const int32_t cw = UI_CONTENT_W - FLOW_W - 3 * UI_PAD;

    lv_obj_t *bat = ui_card(col, "Battery", cw, 150);
    s_soc_lbl = ui_label(bat, &lv_font_montserrat_48, g_pal.text, "--%");
    s_soc_bar = lv_bar_create(bat);
    lv_obj_set_size(s_soc_bar, LV_PCT(100), 18);
    lv_bar_set_range(s_soc_bar, 0, 100);
    lv_obj_set_style_bg_color(s_soc_bar, g_pal.battery, LV_PART_INDICATOR);
    s_bat_state = ui_label(bat, &lv_font_montserrat_16, g_pal.muted, "");

    lv_obj_t *mode = ui_card(col, "Inverter", cw, 110);
    s_mode_lbl = ui_kv(mode, "Run status");
    s_gen_lbl = ui_kv(mode, "Generator");

    lv_obj_t *today = ui_card(col, "Today", cw, 236);
    static const char *const keys[6] = {"PV produced", "Consumed",      "Grid bought",
                                        "Grid sold",   "Batt. charged", "Batt. discharged"};
    for (int i = 0; i < 6; i++) {
        s_today[i] = ui_kv(today, keys[i]);
    }
    s_comm_lbl = ui_note(today, "");
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const inverter_model_t *m = &s->model;
    const bool live = ui_is_live(s);
    const float rated = m->inverter.rated_power_w.valid ? m->inverter.rated_power_w.value : 6000.0f;
    char buf[32];
    app_settings_t st;
    app_settings_get(&st);

    /* PV: always flows towards the inverter */
    fmt_power(buf, sizeof(buf), m->pv.total_power_w, false);
    lv_label_set_text(s_nodes[N_PV].value, buf);
    lv_label_set_text(s_nodes[N_PV].sub, "");
    set_link(&s_nodes[N_PV],
             !m->pv.total_power_w.valid               ? DIR_NONE
             : m->pv.total_power_w.value > MIN_FLOW_W ? DIR_TO_INV
                                                      : DIR_NONE,
             m->pv.total_power_w.value, rated);

    /* Load: always supplied by the inverter */
    fmt_power(buf, sizeof(buf), m->load.power_w, false);
    lv_label_set_text(s_nodes[N_LOAD].value, buf);
    set_link(&s_nodes[N_LOAD], m->load.power_w.valid && m->load.power_w.value > MIN_FLOW_W ? DIR_FROM_INV : DIR_NONE,
             m->load.power_w.value, rated);

    /* Battery: positive = discharging (see docs/api-contract.md) */
    const im_value_t bp = m->battery.power_w;
    fmt_power(buf, sizeof(buf), bp, true);
    lv_label_set_text(s_nodes[N_BAT].value, buf);
    lv_label_set_text(s_nodes[N_BAT].sub, ui_battery_state_text(bp));
    flow_dir_t bdir = DIR_NONE;
    if (bp.valid && fabsf(bp.value) > MIN_FLOW_W) {
        bdir = bp.value > 0 ? DIR_TO_INV : DIR_FROM_INV;
    }
    set_link(&s_nodes[N_BAT], bdir, bp.value, rated);

    /* Grid: direction only if the user configured a verified convention */
    const im_value_t gp = m->grid.power_w;
    const bool grid_off = m->grid.connected.valid && m->grid.connected.value < 0.5f;
    fmt_power(buf, sizeof(buf), gp, true);
    lv_label_set_text(s_nodes[N_GRID].value, grid_off ? "OFF" : buf);
    flow_dir_t gdir = DIR_NONE;
    const char *gsub = "";
    if (grid_off) {
        gsub = "Disconnected";
    } else if (!m->grid.connected.valid) {
        gsub = "State unknown";
    } else if (gp.valid && fabsf(gp.value) > MIN_FLOW_W) {
        if (st.grid_sign == GRID_SIGN_UNVERIFIED) {
            gdir = DIR_UNKNOWN;
            gsub = "Direction unverified";
        } else {
            const bool importing = (st.grid_sign == GRID_SIGN_POSITIVE_IMPORT) == (gp.value > 0);
            gdir = importing ? DIR_TO_INV : DIR_FROM_INV;
            gsub = importing ? "Importing" : "Exporting";
        }
    } else {
        gsub = "Connected";
    }
    lv_label_set_text(s_nodes[N_GRID].sub, gsub);
    set_link(&s_nodes[N_GRID], gdir, gp.value, rated);

    for (int i = 0; i < N_COUNT; i++) {
        ui_value_style(s_nodes[i].value, live);
    }
    lv_label_set_text(s_inv_status, im_run_status_name(m->inverter.run_status));

    /* Side cards */
    fmt_value(buf, sizeof(buf), m->battery.soc_pct, 0, "%");
    lv_label_set_text(s_soc_lbl, buf);
    ui_value_style(s_soc_lbl, live);
    lv_bar_set_value(s_soc_bar, m->battery.soc_pct.valid ? (int32_t)m->battery.soc_pct.value : 0, LV_ANIM_OFF);
    char p[24];
    fmt_power(p, sizeof(p), bp, true);
    lv_label_set_text_fmt(s_bat_state, "%s  %s", ui_battery_state_text(bp), p);

    lv_label_set_text(s_mode_lbl, im_run_status_name(m->inverter.run_status));
    lv_obj_set_style_text_color(s_mode_lbl,
                                m->inverter.run_status == IM_RUN_NORMAL ? g_pal.ok
                                : (m->inverter.run_status == IM_RUN_ALARM || m->inverter.run_status == IM_RUN_FAULT)
                                    ? g_pal.err
                                    : g_pal.muted,
                                0);
    lv_label_set_text(s_gen_lbl, m->generator.reported ? "Reported" : "Not reported");

    const im_value_t today[6] = {m->pv.daily_kwh,        m->load.daily_kwh,           m->grid.daily_bought_kwh,
                                 m->grid.daily_sold_kwh, m->battery.daily_charge_kwh, m->battery.daily_discharge_kwh};
    for (int i = 0; i < 6; i++) {
        fmt_value(buf, sizeof(buf), today[i], 1, "kWh");
        lv_label_set_text(s_today[i], buf);
        ui_value_style(s_today[i], live);
    }
    if (!s->backend_configured) {
        lv_label_set_text(s_comm_lbl, "Backend not configured: open Network.");
    } else if (!live) {
        char age[16];
        fmt_age_ms(age, sizeof(age), link_data_age_ms(&s->link, s->now_ms));
        lv_label_set_text_fmt(s_comm_lbl, "Values are NOT live (age %s). %s", age, s->last_error);
    } else {
        lv_label_set_text(s_comm_lbl, "");
    }
}

const ui_screen_t scr_overview = {T_OVERVIEW, LV_SYMBOL_HOME, false, create, update};
