/* Grid, household load and inverter output. */
#include <math.h>

#include "fmt.h"
#include "ui_internal.h"

enum { G_STATE, G_V, G_F, G_P, G_CT, G_BOUGHT, G_SOLD, G_SOLD_T, G_N };
enum { L_P, L_V, L_I, L_F, L_DAY, L_TOT, L_N };
enum { I_STATUS, I_P, I_V, I_F, I_TDC, I_TAC, I_RATED, I_N };

static lv_obj_t *s_g[G_N], *s_l[L_N], *s_i[I_N];

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    const int32_t w = (UI_CONTENT_W - 4 * UI_PAD) / 3;
    const int32_t h = UI_CONTENT_H - 2 * UI_PAD;

    lv_obj_t *g = ui_card(page, LV_SYMBOL_POWER " Grid", w, h);
    static const char *const gk[G_N] = {"State",    "Voltage",      "Frequency",  "Power",
                                        "CT power", "Bought today", "Sold today", "Sold total"};
    for (int i = 0; i < G_N; i++) {
        s_g[i] = ui_kv(g, gk[i]);
    }
    ui_note(g, "Grid power direction is shown only after the sign convention is set in System.");

    lv_obj_t *l = ui_card(page, LV_SYMBOL_HOME " Load", w, h);
    static const char *const lk[L_N] = {"Power", "Voltage", "Current", "Frequency", "Today", "Total"};
    for (int i = 0; i < L_N; i++) {
        s_l[i] = ui_kv(l, lk[i]);
    }

    lv_obj_t *inv = ui_card(page, LV_SYMBOL_CHARGE " Inverter", w, h);
    static const char *const ik[I_N] = {"Run status", "Output power", "Output voltage", "Output freq.",
                                        "DC temp.",   "AC temp.",     "Rated power"};
    for (int i = 0; i < I_N; i++) {
        s_i[i] = ui_kv(inv, ik[i]);
    }
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const inverter_model_t *m = &s->model;
    const bool live = ui_is_live(s);
    char buf[32];
    app_settings_t st;
    app_settings_get(&st);

    if (!m->grid.connected.valid) {
        lv_label_set_text(s_g[G_STATE], "Unknown");
    } else {
        lv_label_set_text(s_g[G_STATE],
                          m->grid.connected.value > 0.5f ? LV_SYMBOL_OK " Connected" : LV_SYMBOL_CLOSE " Disconnected");
    }
    fmt_value(buf, sizeof(buf), m->grid.voltage_v, 1, "V");
    lv_label_set_text(s_g[G_V], buf);
    fmt_value(buf, sizeof(buf), m->grid.frequency_hz, 2, "Hz");
    lv_label_set_text(s_g[G_F], buf);
    fmt_power(buf, sizeof(buf), m->grid.power_w, st.grid_sign == GRID_SIGN_UNVERIFIED);
    if (m->grid.power_w.valid && fabsf(m->grid.power_w.value) > 5.0f && st.grid_sign != GRID_SIGN_UNVERIFIED) {
        const bool imp = (st.grid_sign == GRID_SIGN_POSITIVE_IMPORT) == (m->grid.power_w.value > 0);
        fmt_power(buf, sizeof(buf), m->grid.power_w, true);
        lv_label_set_text_fmt(s_g[G_P], "%s %s", imp ? "Import" : "Export", buf);
    } else {
        lv_label_set_text(s_g[G_P], buf);
    }
    fmt_power(buf, sizeof(buf), m->grid.ct_power_w, false);
    lv_label_set_text(s_g[G_CT], buf);
    fmt_value(buf, sizeof(buf), m->grid.daily_bought_kwh, 1, "kWh");
    lv_label_set_text(s_g[G_BOUGHT], buf);
    fmt_value(buf, sizeof(buf), m->grid.daily_sold_kwh, 1, "kWh");
    lv_label_set_text(s_g[G_SOLD], buf);
    fmt_value(buf, sizeof(buf), m->grid.total_sold_kwh, 0, "kWh");
    lv_label_set_text(s_g[G_SOLD_T], buf);

    fmt_power(buf, sizeof(buf), m->load.power_w, false);
    lv_label_set_text(s_l[L_P], buf);
    fmt_value(buf, sizeof(buf), m->load.voltage_v, 1, "V");
    lv_label_set_text(s_l[L_V], buf);
    fmt_value(buf, sizeof(buf), m->load.current_a, 2, "A");
    lv_label_set_text(s_l[L_I], buf);
    fmt_value(buf, sizeof(buf), m->load.frequency_hz, 2, "Hz");
    lv_label_set_text(s_l[L_F], buf);
    fmt_value(buf, sizeof(buf), m->load.daily_kwh, 1, "kWh");
    lv_label_set_text(s_l[L_DAY], buf);
    fmt_value(buf, sizeof(buf), m->load.total_kwh, 0, "kWh");
    lv_label_set_text(s_l[L_TOT], buf);

    lv_label_set_text(s_i[I_STATUS], im_run_status_name(m->inverter.run_status));
    fmt_power(buf, sizeof(buf), m->inverter.output_power_w, false);
    lv_label_set_text(s_i[I_P], buf);
    fmt_value(buf, sizeof(buf), m->inverter.output_voltage_v, 1, "V");
    lv_label_set_text(s_i[I_V], buf);
    fmt_value(buf, sizeof(buf), m->inverter.output_frequency_hz, 2, "Hz");
    lv_label_set_text(s_i[I_F], buf);
    fmt_value(buf, sizeof(buf), m->inverter.dc_temperature_c, 1, "C");
    lv_label_set_text(s_i[I_TDC], buf);
    fmt_value(buf, sizeof(buf), m->inverter.ac_temperature_c, 1, "C");
    lv_label_set_text(s_i[I_TAC], buf);
    fmt_power(buf, sizeof(buf), m->inverter.rated_power_w, false);
    lv_label_set_text(s_i[I_RATED], buf);

    for (int i = 0; i < G_N; i++) {
        ui_value_style(s_g[i], live);
    }
    for (int i = 0; i < L_N; i++) {
        ui_value_style(s_l[i], live);
    }
    for (int i = 0; i < I_N; i++) {
        ui_value_style(s_i[i], live);
    }
}

const ui_screen_t scr_grid = {T_GRID_LOAD, LV_SYMBOL_POWER, false, create, update};
