/* Interactive history charts backed by the bounded on-device history. */
#include "fmt.h"
#include "ui_internal.h"

static ui_chart_t s_chart;
static lv_obj_t *s_series_btns[HIST_SERIES_COUNT];
static lv_obj_t *s_range_btns[HIST_RANGE_COUNT];
static hist_series_t s_series = HIST_PV;
static hist_range_t s_range = HIST_RANGE_1H;
static bool s_dirty = true;
static lv_obj_t *s_today[6];

static const char *const k_series_names[HIST_SERIES_COUNT] = {"PV", "Load", "Grid", "Battery", "SOC"};
static const char *const k_range_names[HIST_RANGE_COUNT] = {"1 h", "6 h", "24 h", "7 d"};

static lv_color_t series_color(hist_series_t s)
{
    switch (s) {
    case HIST_PV:
        return g_pal.pv;
    case HIST_LOAD:
        return g_pal.load;
    case HIST_GRID:
        return g_pal.grid;
    default:
        return g_pal.battery;
    }
}

static void sync_buttons(void)
{
    for (int i = 0; i < HIST_SERIES_COUNT; i++) {
        if (i == (int)s_series) {
            lv_obj_add_state(s_series_btns[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_series_btns[i], LV_STATE_CHECKED);
        }
    }
    for (int i = 0; i < HIST_RANGE_COUNT; i++) {
        if (i == (int)s_range) {
            lv_obj_add_state(s_range_btns[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_range_btns[i], LV_STATE_CHECKED);
        }
    }
    ui_chart_set_color(&s_chart, series_color(s_series));
}

static void series_cb(lv_event_t *e)
{
    s_series = (hist_series_t)(intptr_t)lv_event_get_user_data(e);
    s_dirty = true;
    sync_buttons();
}

static void range_cb(lv_event_t *e)
{
    s_range = (hist_range_t)(intptr_t)lv_event_get_user_data(e);
    s_dirty = true;
    sync_buttons();
}

static lv_obj_t *toggle(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, intptr_t v)
{
    lv_obj_t *bt = ui_button(parent, txt, cb, (void *)v);
    lv_obj_set_style_min_width(bt, 96, 0);
    lv_obj_set_style_bg_color(bt, g_pal.inactive, 0);
    lv_obj_set_style_bg_color(bt, g_pal.accent, LV_STATE_CHECKED);
    return bt;
}

static void create(lv_obj_t *page)
{
    ui_flex_col(page, UI_PAD);
    lv_obj_t *bar = lv_obj_create(page);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), 56);
    ui_flex_row(bar, 6);
    for (int i = 0; i < HIST_SERIES_COUNT; i++) {
        s_series_btns[i] = toggle(bar, k_series_names[i], series_cb, i);
    }
    lv_obj_t *sp = lv_obj_create(bar);
    lv_obj_remove_style_all(sp);
    lv_obj_set_size(sp, 4, 1);
    for (int i = 0; i < HIST_RANGE_COUNT; i++) {
        s_range_btns[i] = toggle(bar, k_range_names[i], range_cb, i);
        lv_obj_set_style_min_width(s_range_btns[i], 72, 0);
    }
    lv_obj_t *c = ui_card(page, NULL, UI_CONTENT_W - 2 * UI_PAD, 380);
    ui_chart_create(&s_chart, c, UI_CONTENT_W - 4 * UI_PAD - 2, 350, 180, g_pal.pv);
    lv_obj_t *today = ui_card(page, NULL, UI_CONTENT_W - 2 * UI_PAD, 136);
    lv_obj_set_layout(today, LV_LAYOUT_NONE);
    lv_obj_t *heading = ui_label(today, &lv_font_montserrat_16, g_pal.muted, "Today");
    lv_obj_set_pos(heading, 4, 0);
    static const char *const names[6] = {"PV produced", "Consumed",        "Grid bought",
                                         "Grid sold",   "Battery charged", "Battery discharged"};
    for (int i = 0; i < 6; i++) {
        const int32_t x = 4 + (i % 3) * 322;
        const int32_t y = 34 + (i / 3) * 50;
        lv_obj_t *key = ui_label(today, &lv_font_montserrat_14, g_pal.muted, names[i]);
        lv_obj_set_pos(key, x, y);
        s_today[i] = ui_label(today, &lv_font_montserrat_20, g_pal.text, "--");
        lv_obj_set_pos(s_today[i], x, y + 17);
    }
    ui_note(page, "History is collected on this display since boot (1-min resolution for 24 h, 15-min for "
                  "7 days) and is lost on restart. Long-term history requires a backend time-series store "
                  "(e.g. Prometheus server), which is not available on the configured backend.");
    sync_buttons();
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (changed) {
        const inverter_model_t *m = &s->model;
        const im_value_t values[6] = {
            m->pv.daily_kwh,        m->load.daily_kwh,           m->grid.daily_bought_kwh,
            m->grid.daily_sold_kwh, m->battery.daily_charge_kwh, m->battery.daily_discharge_kwh};
        for (int i = 0; i < 6; i++) {
            if (ui_is_live(s)) {
                char buf[24];
                fmt_value(buf, sizeof(buf), values[i], 1, "kWh");
                lv_label_set_text(s_today[i], buf);
            } else {
                lv_label_set_text(s_today[i], "--");
            }
        }
    }
    if (!changed && !s_dirty) {
        return;
    }
    if (ui_chart_fill(&s_chart, s_range, s_series, s->now_ms)) {
        s_dirty = false;
    }
}

const ui_screen_t scr_charts = {T_CHARTS, LV_SYMBOL_LIST, false, create, update};
