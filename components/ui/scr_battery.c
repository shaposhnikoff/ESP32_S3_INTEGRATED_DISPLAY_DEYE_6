/* Battery details and charge/discharge history. */
#include "fmt.h"
#include "ui_internal.h"

static lv_obj_t *s_soc, *s_bar, *s_state;
static lv_obj_t *s_kv[9];
static ui_chart_t s_chart;

static void create(lv_obj_t *page)
{
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(page, UI_PAD, 0);
    lv_obj_set_style_pad_column(page, UI_PAD, 0);

    lv_obj_t *soc = ui_card(page, "State of charge", 280, 250);
    s_soc = ui_label(soc, &lv_font_montserrat_48, g_pal.text, "--%");
    s_bar = lv_bar_create(soc);
    lv_obj_set_size(s_bar, LV_PCT(100), 28);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_set_style_bg_color(s_bar, g_pal.battery, LV_PART_INDICATOR);
    s_state = ui_label(soc, &lv_font_montserrat_24, g_pal.text, "");
    ui_note(soc, "Aggregate battery data. Module-level BMS data is not exported by the backend.");

    lv_obj_t *d = ui_card(page, "Measurements", UI_CONTENT_W - 280 - 3 * UI_PAD, 250);
    lv_obj_t *columns = lv_obj_create(d);
    lv_obj_remove_style_all(columns);
    lv_obj_set_size(columns, LV_PCT(100), LV_SIZE_CONTENT);
    ui_flex_row(columns, UI_PAD);
    lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *col[2];
    for (int i = 0; i < 2; i++) {
        col[i] = lv_obj_create(columns);
        lv_obj_remove_style_all(col[i]);
        lv_obj_set_size(col[i], LV_PCT(48), LV_SIZE_CONTENT);
        ui_flex_col(col[i], 8);
    }
    static const char *const keys[9] = {"Voltage",      "Current",         "Power",
                                        "Temperature",  "Charge today",    "Discharge today",
                                        "Charge total", "Discharge total", "Sign"};
    for (int i = 0; i < 9; i++) {
        s_kv[i] = ui_kv(col[i >= 4 && i < 8], keys[i]);
        lv_obj_t *row = lv_obj_get_parent(s_kv[i]);
        lv_obj_set_style_text_font(lv_obj_get_child(row, 0), &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_font(s_kv[i], &lv_font_montserrat_16, 0);
    }

    lv_obj_t *c = ui_card(page, "Battery power, last 6 h (+ discharge / - charge)", UI_CONTENT_W - 2 * UI_PAD, 250);
    ui_chart_create(&s_chart, c, UI_CONTENT_W - 4 * UI_PAD - 2, 190, 120, g_pal.battery);
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const im_battery_t *b = &s->model.battery;
    const bool live = ui_is_live(s);
    char buf[32];
    fmt_value(buf, sizeof(buf), b->soc_pct, 0, "%");
    lv_label_set_text(s_soc, buf);
    ui_value_style(s_soc, live);
    lv_bar_set_value(s_bar, b->soc_pct.valid ? (int32_t)b->soc_pct.value : 0, LV_ANIM_OFF);
    lv_label_set_text(s_state, ui_battery_state_text(b->power_w));

    fmt_value(buf, sizeof(buf), b->voltage_v, 2, "V");
    lv_label_set_text(s_kv[0], buf);
    fmt_value(buf, sizeof(buf), b->current_a, 1, "A");
    lv_label_set_text(s_kv[1], buf);
    fmt_power(buf, sizeof(buf), b->power_w, false);
    lv_label_set_text(s_kv[2], buf);
    fmt_value(buf, sizeof(buf), b->temperature_c, 1, "C");
    lv_label_set_text(s_kv[3], buf);
    fmt_value(buf, sizeof(buf), b->daily_charge_kwh, 1, "kWh");
    lv_label_set_text(s_kv[4], buf);
    fmt_value(buf, sizeof(buf), b->daily_discharge_kwh, 1, "kWh");
    lv_label_set_text(s_kv[5], buf);
    fmt_value(buf, sizeof(buf), b->total_charge_kwh, 0, "kWh");
    lv_label_set_text(s_kv[6], buf);
    fmt_value(buf, sizeof(buf), b->total_discharge_kwh, 0, "kWh");
    lv_label_set_text(s_kv[7], buf);
    lv_label_set_text(s_kv[8], "+ = discharge");
    for (int i = 0; i < 8; i++) {
        ui_value_style(s_kv[i], live);
    }
    ui_chart_fill(&s_chart, HIST_RANGE_6H, HIST_BATTERY, s->now_ms);
}

const ui_screen_t scr_battery = {T_BATTERY, LV_SYMBOL_BATTERY_FULL, false, create, update};
