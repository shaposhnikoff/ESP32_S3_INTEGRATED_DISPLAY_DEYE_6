/* PV generation per string and history. */
#include "fmt.h"
#include "ui_internal.h"

static lv_obj_t *s_total, *s_daily, *s_lifetime;
static lv_obj_t *s_table;
static lv_obj_t *s_note;
static ui_chart_t s_chart;

static void create(lv_obj_t *page)
{
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(page, UI_PAD, 0);
    lv_obj_set_style_pad_column(page, UI_PAD, 0);

    lv_obj_t *sum = ui_card(page, "PV generation", 280, 250);
    s_total = ui_label(sum, &lv_font_montserrat_48, g_pal.pv, "--");
    s_daily = ui_kv(sum, "Today");
    s_lifetime = ui_kv(sum, "Lifetime");

    lv_obj_t *st = ui_card(page, "Strings (MPPT inputs)", UI_CONTENT_W - 280 - 3 * UI_PAD, 250);
    s_table = lv_table_create(st);
    lv_table_set_column_count(s_table, 4);
    lv_table_set_row_count(s_table, 1);
    static const char *const hdr[4] = {"String", "Voltage", "Current", "Power"};
    for (int c = 0; c < 4; c++) {
        lv_table_set_cell_value(s_table, 0, (uint32_t)c, hdr[c]);
        lv_table_set_column_width(s_table, (uint32_t)c, 130);
    }
    lv_obj_set_style_text_font(s_table, &lv_font_montserrat_20, LV_PART_ITEMS);
    s_note = ui_note(st, "");

    lv_obj_t *c = ui_card(page, "PV power, last 24 h", UI_CONTENT_W - 2 * UI_PAD, 250);
    ui_chart_create(&s_chart, c, UI_CONTENT_W - 4 * UI_PAD - 2, 190, 144, g_pal.pv);
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const im_pv_t *pv = &s->model.pv;
    const bool live = ui_is_live(s);
    char buf[32];
    fmt_power(buf, sizeof(buf), pv->total_power_w, false);
    lv_label_set_text(s_total, buf);
    ui_value_style(s_total, live);
    fmt_value(buf, sizeof(buf), pv->daily_kwh, 1, "kWh");
    lv_label_set_text(s_daily, buf);
    fmt_value(buf, sizeof(buf), pv->total_kwh, 0, "kWh");
    lv_label_set_text(s_lifetime, buf);

    /* Show only as many strings as the inverter reports MPPT inputs. */
    int n = IM_PV_STRINGS;
    const im_value_t mppts = s->model.inverter.mppts;
    if (mppts.valid && mppts.value >= 1 && mppts.value <= IM_PV_STRINGS) {
        n = (int)mppts.value;
        lv_label_set_text_fmt(s_note, "Inverter reports %d MPPT inputs.", n);
    } else {
        lv_label_set_text(s_note, "MPPT count unknown: showing all exported strings.");
    }
    lv_table_set_row_count(s_table, (uint32_t)n + 1);
    for (int i = 0; i < n; i++) {
        const im_pv_string_t *str = &pv->strings[i];
        lv_table_set_cell_value_fmt(s_table, (uint32_t)i + 1, 0, "PV%d", i + 1);
        fmt_value(buf, sizeof(buf), str->voltage_v, 1, "V");
        lv_table_set_cell_value(s_table, (uint32_t)i + 1, 1, buf);
        fmt_value(buf, sizeof(buf), str->current_a, 1, "A");
        lv_table_set_cell_value(s_table, (uint32_t)i + 1, 2, buf);
        fmt_power(buf, sizeof(buf), str->power_w, false);
        lv_table_set_cell_value(s_table, (uint32_t)i + 1, 3, buf);
    }
    ui_chart_fill(&s_chart, HIST_RANGE_24H, HIST_PV, s->now_ms);
}

const ui_screen_t scr_solar = {T_SOLAR, LV_SYMBOL_IMAGE, false, create, update};
