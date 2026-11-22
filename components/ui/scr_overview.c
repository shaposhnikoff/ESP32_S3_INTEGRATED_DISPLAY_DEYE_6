/* Seven-inch overview with a 24-hour local outage dial. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fmt.h"
#include "outage_dial.h"
#include "ui_internal.h"

#define HEADER_Y 16
#define HEADER_H 100
#define CARD_Y 128
#define CARD_H 308
#define TILE_Y 448
#define TILE_H 72
#define ARC_MAX (OUTAGE_MAX_INTERVALS * 2)

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *value;
} overview_tile_t;

static lv_obj_t *s_header;
static lv_obj_t *s_header_badge;
static lv_obj_t *s_header_icon;
static lv_obj_t *s_headline;
static lv_obj_t *s_dial;
static lv_obj_t *s_ring;
static lv_obj_t *s_arcs[ARC_MAX];
static lv_obj_t *s_ticks;
static lv_obj_t *s_marker;
static lv_point_precise_t s_marker_points[2];
static lv_obj_t *s_soc;
static lv_obj_t *s_clock;
static lv_obj_t *s_event_label;
static lv_obj_t *s_event_time;
static lv_obj_t *s_event_tomorrow;
static lv_obj_t *s_countdown;
static lv_obj_t *s_coverage;
static lv_obj_t *s_next;
static lv_obj_t *s_inverter_status;
static lv_obj_t *s_inverter_badge;
static lv_obj_t *s_inverter_icon;
static lv_obj_t *s_load;
static lv_obj_t *s_data_dot;
static overview_tile_t s_tiles[6];
static int64_t s_now_override = -1;

void ui_overview_set_now(int64_t epoch)
{
    s_now_override = epoch;
}

static lv_color_t green(void)
{
    return lv_color_hex(0x3DDC84);
}
static lv_color_t amber(void)
{
    return lv_color_hex(0xFFB020);
}
static lv_color_t red(void)
{
    return lv_color_hex(0xFF5C5C);
}
static lv_color_t gray(void)
{
    return lv_color_hex(0x9AA9B1);
}
static lv_color_t muted(void)
{
    return lv_color_hex(0xB4C4CC);
}
static lv_color_t card_color(void)
{
    return lv_color_hex(0x121F26);
}
static lv_color_t border_color(void)
{
    return lv_color_hex(0x22343D);
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
    lv_obj_set_style_bg_color(obj, card_color(), 0);
    lv_obj_set_style_border_color(obj, border_color(), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 18, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static overview_tile_t tile(lv_obj_t *page, int32_t x, int32_t w, const char *title)
{
    overview_tile_t t;
    t.card = panel(page, x, TILE_Y, w, TILE_H);
    lv_obj_set_style_border_width(t.card, 0, 0);
    t.title = label_at(t.card, &lv_font_montserrat_14, muted(), title, 12, 9);
    t.value = label_at(t.card, &deye_font_tile, lv_color_white(), "--", 12, 31);
    return t;
}

static lv_obj_t *ring_arc(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 234, 234);
    lv_obj_set_pos(arc, 18, 18);
    lv_arc_set_rotation(arc, 270);
    lv_obj_set_style_arc_width(arc, 26, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 26, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

static void draw_hour_ticks(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    const int center_x = area.x1 + 135;
    const int center_y = area.y1 + 135;
    lv_layer_t *layer = lv_event_get_layer(event);
    for (int hour = 0; hour < 24; hour++) {
        const float angle = (float)hour * 3.14159265f / 12.0f - 3.14159265f / 2.0f;
        const int inner = hour % 6 == 0 ? 101 : 108;
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = hour % 6 == 0 ? lv_color_hex(0xE4EFF2) : lv_color_hex(0x96ABB4);
        line.width = hour % 6 == 0 ? 3 : 2;
        line.p1.x = center_x + (int)(inner * cosf(angle));
        line.p1.y = center_y + (int)(inner * sinf(angle));
        line.p2.x = center_x + (int)(117 * cosf(angle));
        line.p2.y = center_y + (int)(117 * sinf(angle));
        lv_draw_line(layer, &line);
    }
}

static void create_dial(lv_obj_t *battery)
{
    s_dial = panel(battery, 20, 18, 270, 270);
    lv_obj_set_style_bg_opa(s_dial, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dial, 0, 0);
    s_ring = ring_arc(s_dial, lv_color_hex(0x1F5A3A));
    lv_arc_set_angles(s_ring, 0, 360);
    for (int i = 0; i < ARC_MAX; i++) {
        s_arcs[i] = ring_arc(s_dial, amber());
        lv_obj_add_flag(s_arcs[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_ticks = lv_obj_create(s_dial);
    lv_obj_set_size(s_ticks, 270, 270);
    lv_obj_set_pos(s_ticks, 0, 0);
    lv_obj_set_style_bg_opa(s_ticks, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ticks, 0, 0);
    lv_obj_set_style_pad_all(s_ticks, 0, 0);
    lv_obj_remove_flag(s_ticks, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ticks, draw_hour_ticks, LV_EVENT_DRAW_MAIN, NULL);
    label_at(s_dial, &lv_font_montserrat_14, muted(), "00", 124, 0);
    label_at(s_dial, &lv_font_montserrat_14, muted(), "06", 246, 124);
    label_at(s_dial, &lv_font_montserrat_14, muted(), "12", 124, 249);
    label_at(s_dial, &lv_font_montserrat_14, muted(), "18", 0, 124);
    s_marker = lv_line_create(s_dial);
    lv_line_set_points_mutable(s_marker, s_marker_points, 2);
    lv_obj_set_style_line_width(s_marker, 5, 0);
    lv_obj_set_style_line_color(s_marker, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(s_marker, true, 0);
    lv_obj_add_flag(s_marker, LV_OBJ_FLAG_HIDDEN);
    s_soc = ui_label(s_dial, &deye_font_dial_soc, green(), "--");
    lv_obj_align(s_soc, LV_ALIGN_CENTER, 0, -17);
    label_at(s_dial, &lv_font_montserrat_14, muted(), "BATTERY", 100, 161);
    s_clock = ui_label(s_dial, &lv_font_montserrat_14, muted(), "24H  --:--");
    lv_obj_align(s_clock, LV_ALIGN_CENTER, 0, 53);
}

static void create(lv_obj_t *page)
{
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x0A1318), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    s_header = panel(page, 16, HEADER_Y, 992, HEADER_H);
    lv_obj_set_style_border_width(s_header, 3, 0);
    s_header_badge = panel(s_header, 0, 20, 60, 60);
    lv_obj_set_style_radius(s_header_badge, 30, 0);
    lv_obj_set_style_bg_opa(s_header_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_header_badge, 5, 0);
    s_header_icon = ui_label(s_header_badge, &lv_font_montserrat_36, green(), LV_SYMBOL_OK);
    lv_obj_center(s_header_icon);
    s_headline = label_at(s_header, &deye_font_inverter, lv_color_white(), "NO DATA", 160, 6);

    lv_obj_t *battery = panel(page, 16, CARD_Y, 490, CARD_H);
    create_dial(battery);
    s_event_label = label_at(battery, &lv_font_montserrat_14, muted(), "SCHEDULE UNAVAILABLE", 303, 59);
    s_event_time = label_at(battery, &deye_font_dial_time, lv_color_white(), "", 303, 80);
    s_event_tomorrow = label_at(battery, &lv_font_montserrat_14, muted(), "", 305, 145);
    s_countdown = label_at(battery, &lv_font_montserrat_24, amber(), "", 303, 158);
    s_coverage = label_at(battery, &lv_font_montserrat_14, green(), "", 303, 199);
    s_next = label_at(battery, &lv_font_montserrat_14, muted(), "", 303, 258);

    lv_obj_t *inverter = panel(page, 518, CARD_Y, 490, CARD_H);
    label_at(inverter, &lv_font_montserrat_24, muted(), LV_SYMBOL_CHARGE "  INVERTER", 24, 18);
    s_inverter_badge = panel(inverter, 430, 16, 34, 34);
    lv_obj_set_style_radius(s_inverter_badge, 17, 0);
    lv_obj_set_style_bg_opa(s_inverter_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_inverter_badge, 3, 0);
    s_inverter_icon = ui_label(s_inverter_badge, &lv_font_montserrat_24, green(), LV_SYMBOL_OK);
    lv_obj_center(s_inverter_icon);
    s_inverter_status = label_at(inverter, &deye_font_inverter, green(), "N/A", 24, 92);
    lv_obj_t *rule = panel(inverter, 24, 234, 438, 1);
    lv_obj_set_style_bg_color(rule, border_color(), 0);
    lv_obj_set_style_border_width(rule, 0, 0);
    lv_obj_set_style_radius(rule, 0, 0);
    label_at(inverter, &lv_font_montserrat_24, muted(), "Load", 24, 260);
    s_load = ui_label(inverter, &deye_font_value, lv_color_white(), "--");
    lv_obj_align(s_load, LV_ALIGN_TOP_RIGHT, -24, 243);

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
    lv_obj_t *legend = panel(page, 16, 532, 992, 52);
    label_at(legend, &lv_font_montserrat_14, muted(),
             "24H SCHEDULE  |  GREEN: GRID AVAILABLE  |  AMBER: OUTAGE  |  WHITE: NOW", 20, 17);
}

static void set_header(const char *text, const char *icon, lv_color_t color, lv_color_t bg)
{
    lv_label_set_text(s_headline, text);
    lv_obj_set_style_text_font(s_headline, &deye_font_inverter, 0);
    lv_obj_set_style_bg_color(s_header, bg, 0);
    lv_obj_set_style_border_color(s_header, color, 0);
    lv_obj_set_style_border_color(s_header_badge, color, 0);
    lv_obj_set_style_text_color(s_header_icon, color, 0);
    lv_label_set_text(s_header_icon, icon);
    lv_obj_update_layout(s_headline);
    int width = lv_obj_get_width(s_headline);
    int x = (992 - 60 - 24 - width) / 2;
    lv_obj_set_x(s_header_badge, x);
    lv_obj_set_x(s_headline, x + 84);
    lv_obj_set_y(s_headline, 5);
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

static float average_load(const app_snapshot_t *s)
{
    float result = s->model.load.power_w.valid ? s->model.load.power_w.value : -1.0f;
    if (!app_state_lock(0))
        return result;
    history_t *history = app_state_history();
    if (history != NULL) {
        float values[60];
        bool valid[60];
        const size_t count = hist_query(history, HIST_RANGE_1H, s->now_ms / 1000, HIST_LOAD, values, valid, 60);
        float sum = 0;
        int samples = 0;
        for (size_t i = count > 5 ? count - 5 : 0; i < count; i++) {
            if (valid[i] && values[i] >= 0) {
                sum += values[i];
                samples++;
            }
        }
        if (samples > 0)
            result = sum / samples;
    }
    app_state_unlock();
    return result;
}

static bool local_clock(time_t *epoch, struct tm *local)
{
    *epoch = s_now_override >= 0 ? (time_t)s_now_override : time(NULL);
    if (*epoch < 1704067200)
        return false;
    return localtime_r(epoch, local) != NULL;
}

static void update_dial(const app_snapshot_t *s, bool live, int runtime)
{
    time_t now;
    struct tm local;
    bool clock_ok = local_clock(&now, &local);
    if (clock_ok)
        lv_label_set_text_fmt(s_clock, "24H  %02d:%02d", local.tm_hour, local.tm_min);
    else
        lv_label_set_text(s_clock, "24H  --:--");
    outage_day_t today = {0};
    outage_day_t tomorrow = {0};
    bool available = false;
    if (clock_ok) {
        struct tm next_date = local;
        next_date.tm_mday++;
        next_date.tm_hour = 12;
        next_date.tm_isdst = -1;
        mktime(&next_date);
        available = outage_get_day(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, now, &today);
        if (available)
            outage_get_day(next_date.tm_year + 1900, next_date.tm_mon + 1, next_date.tm_mday, now, &tomorrow);
    }
    lv_obj_set_style_arc_color(s_ring, available ? lv_color_hex(0x1F5A3A) : border_color(), LV_PART_INDICATOR);
    for (int i = 0; i < ARC_MAX; i++)
        lv_obj_add_flag(s_arcs[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_marker, LV_OBJ_FLAG_HIDDEN);
    if (!available) {
        lv_label_set_text(s_event_label, "SCHEDULE\nUNAVAILABLE");
        lv_obj_set_y(s_event_label, 42);
        lv_obj_set_style_text_font(s_event_time, &lv_font_montserrat_48, 0);
        lv_label_set_text(s_event_time, "N/A");
        lv_obj_set_style_text_color(s_event_time, gray(), 0);
        lv_label_set_text(s_event_tomorrow, "");
        lv_label_set_text(s_countdown, "");
        lv_label_set_text(s_coverage, "");
        lv_label_set_text(s_next, "No fresh outage plan");
        return;
    }
    int arc_index = 0;
    for (uint8_t i = 0; i < today.count; i++) {
        outage_arc_t arcs[2];
        size_t pieces = outage_split_arc(today.intervals[i], arcs);
        for (size_t j = 0; j < pieces && arc_index < ARC_MAX; j++) {
            lv_arc_set_angles(s_arcs[arc_index], arcs[j].start_deg - 270, arcs[j].end_deg - 270);
            lv_obj_remove_flag(s_arcs[arc_index++], LV_OBJ_FLAG_HIDDEN);
        }
    }
    const int minute = local.tm_hour * 60 + local.tm_min;
    const float angle = (float)minute * 2.0f * 3.14159265f / 1440.0f - 3.14159265f / 2.0f;
    s_marker_points[0] = (lv_point_precise_t){135 + (int)(84 * cosf(angle)), 135 + (int)(84 * sinf(angle))};
    s_marker_points[1] = (lv_point_precise_t){135 + (int)(122 * cosf(angle)), 135 + (int)(122 * sinf(angle))};
    lv_line_set_points_mutable(s_marker, s_marker_points, 2);
    lv_obj_remove_flag(s_marker, LV_OBJ_FLAG_HIDDEN);

    bool off_now;
    int event, duration, next_start, next_end;
    if (!outage_find_event(&today, &tomorrow, minute, &off_now, &event, &duration, &next_start, &next_end)) {
        lv_label_set_text(s_event_label, "NEXT OUTAGE");
        lv_obj_set_style_text_font(s_event_time, &lv_font_montserrat_48, 0);
        lv_label_set_text(s_event_time, "--");
        lv_label_set_text(s_countdown, "");
        lv_label_set_text(s_coverage, "");
        lv_label_set_text(s_next, "No further confirmed outages");
        return;
    }
    lv_label_set_text(s_event_label, off_now ? "GRID BACK" : "NEXT OUTAGE");
    lv_obj_set_y(s_event_label, 59);
    lv_obj_set_style_text_font(s_event_time, &deye_font_dial_time, 0);
    lv_label_set_text_fmt(s_event_time, "%02d:%02d", (event % 1440) / 60, event % 60);
    lv_obj_set_style_text_color(s_event_time, lv_color_white(), 0);
    lv_label_set_text(s_event_tomorrow, event >= 1440 ? "tomorrow" : "");
    char countdown[32];
    outage_format_duration(countdown, sizeof(countdown), event - minute, true);
    lv_label_set_text(s_countdown, countdown);
    if (next_start >= 0) {
        lv_label_set_text_fmt(s_next, "Next: %02d:%02d-%02d:%02d", (next_start % 1440) / 60, next_start % 60,
                              (next_end % 1440) / 60, next_end % 60);
    } else {
        lv_label_set_text(s_next, "");
    }
    if (!live || runtime < 0) {
        lv_label_set_text(s_coverage, "");
        return;
    }
    int shortfall = outage_shortfall_minutes(runtime, duration, 15);
    if (shortfall == 0) {
        lv_label_set_text(s_coverage, LV_SYMBOL_OK " BATTERY\n    COVERS IT");
        lv_obj_set_style_text_color(s_coverage, green(), 0);
    } else {
        char formatted[24];
        outage_format_duration(formatted, sizeof(formatted), shortfall, false);
        lv_label_set_text_fmt(s_coverage, LV_SYMBOL_CLOSE " WON'T LAST\n    ~%s short", formatted);
        lv_obj_set_style_text_color(s_coverage, red(), 0);
    }
    (void)s;
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed)
        return;
    int64_t age = link_data_age_ms(&s->link, s->now_ms);
    bool live = s->model_valid && s->link.backend_poll_ok && age >= 0 && age <= 30000;
    bool grid_on = s->model.grid.connected.valid && s->model.grid.connected.value >= 0.5f;
    bool fault = s->model.inverter.run_status == IM_RUN_FAULT;
    im_value_t soc = s->model.battery.soc_pct;
    app_settings_t settings;
    app_settings_get(&settings);
    outage_status_t status = outage_status(live, fault, soc.valid, soc.value, settings.battery_warning_pct,
                                           settings.battery_cutoff_pct, grid_on);
    lv_color_t status_color = status == OUTAGE_STATUS_NO_DATA                                     ? gray()
                              : status == OUTAGE_STATUS_FAULT || status == OUTAGE_STATUS_CRITICAL ? red()
                              : status == OUTAGE_STATUS_LOW                                       ? amber()
                                                                                                  : green();
    const char *headline = status == OUTAGE_STATUS_NO_DATA                                   ? "NO DATA"
                           : status == OUTAGE_STATUS_FAULT                                   ? "INVERTER FAULT"
                           : status == OUTAGE_STATUS_CRITICAL || status == OUTAGE_STATUS_LOW ? "LOW BATTERY"
                           : status == OUTAGE_STATUS_GRID                                    ? "ON GRID"
                                                                                             : "ON BATTERY";
    const char *icon = status == OUTAGE_STATUS_NO_DATA                                   ? "?"
                       : status == OUTAGE_STATUS_FAULT                                   ? LV_SYMBOL_CLOSE
                       : status == OUTAGE_STATUS_CRITICAL || status == OUTAGE_STATUS_LOW ? LV_SYMBOL_WARNING
                                                                                         : LV_SYMBOL_OK;
    lv_color_t tint = status == OUTAGE_STATUS_NO_DATA                                     ? lv_color_hex(0x202B30)
                      : status == OUTAGE_STATUS_FAULT || status == OUTAGE_STATUS_CRITICAL ? lv_color_hex(0x352024)
                      : status == OUTAGE_STATUS_LOW                                       ? lv_color_hex(0x352D1A)
                                                                                          : lv_color_hex(0x102E26);
    set_header(headline, icon, status_color, tint);
    lv_color_t soc_color = !live || !soc.valid                        ? gray()
                           : soc.value < settings.battery_cutoff_pct  ? red()
                           : soc.value < settings.battery_warning_pct ? amber()
                                                                      : green();
    lv_obj_set_style_text_color(s_soc, soc_color, 0);
    if (soc.valid)
        lv_label_set_text_fmt(s_soc, "%.0f%%", (double)soc.value);
    else
        lv_label_set_text(s_soc, "--");
    lv_obj_align(s_soc, LV_ALIGN_CENTER, 0, -17);

    im_run_status_t run = live ? s->model.inverter.run_status : IM_RUN_UNKNOWN;
    lv_color_t inverter_color = run == IM_RUN_NORMAL  ? green()
                                : run == IM_RUN_FAULT ? red()
                                : run == IM_RUN_ALARM ? amber()
                                                      : gray();
    lv_label_set_text(s_inverter_status, run_text(run));
    lv_obj_set_style_text_color(s_inverter_status, inverter_color, 0);
    lv_obj_set_style_text_color(s_inverter_icon, inverter_color, 0);
    lv_obj_set_style_border_color(s_inverter_badge, inverter_color, 0);
    lv_label_set_text(s_inverter_icon, run == IM_RUN_NORMAL  ? LV_SYMBOL_OK
                                       : run == IM_RUN_FAULT ? LV_SYMBOL_CLOSE
                                       : run == IM_RUN_ALARM ? LV_SYMBOL_WARNING
                                                             : "?");
    set_power(s_load, s->model.load.power_w, live, false);

    char age_text[16];
    fmt_age_ms(age_text, sizeof(age_text), age);
    const char *data = !s->backend_configured ? "Setup" : live ? "Live" : "Lost";
    lv_label_set_text_fmt(s_tiles[0].value, "%s%s%s", data, age_text[0] == '-' ? "" : " ",
                          age_text[0] == '-' ? "" : age_text);
    lv_obj_set_style_bg_color(s_data_dot, !live ? red() : age > 10000 ? amber() : green(), 0);
    lv_obj_set_style_text_color(s_tiles[0].value, live && age > 10000 ? amber() : lv_color_white(), 0);
    im_value_t bp = s->model.battery.power_w;
    const char *battery_state = fault            ? "IDLE"
                                : !bp.valid      ? "BATTERY"
                                : bp.value > 20  ? "DISCHARGING"
                                : bp.value < -20 ? "CHARGING"
                                                 : "IDLE";
    lv_label_set_text(s_tiles[1].title, battery_state);
    set_power(s_tiles[1].value, bp, live && !fault, true);
    if (fault && live)
        lv_label_set_text(s_tiles[1].value, "0 W");
    float load_avg = live ? average_load(s) : -1.0f;
    int runtime = live && soc.valid && load_avg >= 0 ? outage_runtime_minutes(soc.value, settings.battery_cutoff_pct,
                                                                              settings.battery_capacity_wh, load_avg)
                                                     : -1;
    if (fault)
        runtime = -1;
    lv_label_set_text(s_tiles[2].title, "RUNTIME*");
    char runtime_text[32];
    outage_format_duration(runtime_text, sizeof(runtime_text), runtime, false);
    lv_label_set_text(s_tiles[2].value, runtime_text);
    set_power(s_tiles[3].value, s->model.pv.total_power_w, live, false);
    lv_label_set_text(s_tiles[4].value, !live || !s->model.grid.connected.valid ? "--" : grid_on ? "ON" : "OFF");
    lv_obj_set_style_text_color(s_tiles[4].value, !live ? gray() : grid_on ? green() : status_color, 0);
    if (live && s->wifi.status == WIFI_ST_CONNECTED) {
        lv_label_set_text_fmt(s_tiles[5].value, "%d dBm", s->wifi.rssi);
    } else {
        lv_label_set_text(s_tiles[5].value, "--");
    }
    for (int i = 1; i < 6; i++) {
        if (i != 4)
            lv_obj_set_style_text_color(s_tiles[i].value, live ? lv_color_white() : gray(), 0);
    }
    update_dial(s, live, runtime);
}

const ui_screen_t scr_overview = {T_OVERVIEW, LV_SYMBOL_HOME, false, create, update};
