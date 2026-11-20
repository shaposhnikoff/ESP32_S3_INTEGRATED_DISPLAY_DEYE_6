/*
 * Inverter controls. Every candidate parameter is shown with its current
 * value, range, persistence class and capability status. Editing is gated
 * by ctrl_can_edit(): as shipped no parameter is verified and the backend
 * has no write API, so every edit button is disabled with the reason shown.
 * The edit/confirm flow below is the only path to a write.
 */
#include <stdio.h>
#include <stdlib.h>

#include "control.h"
#include "fmt.h"
#include "ui_internal.h"

static lv_obj_t *s_tou_table;
static lv_obj_t *s_tou_days;
static lv_obj_t *s_rows_cur[CTRL_PARAM_COUNT];
static lv_obj_t *s_rows_status[CTRL_PARAM_COUNT];
static lv_obj_t *s_rows_btn[CTRL_PARAM_COUNT];
static lv_obj_t *s_cmd_lbl;
static lv_obj_t *s_ack_btn;

static void fmt_param(char *buf, size_t n, const ctrl_param_desc_t *d, im_value_t v)
{
    if (d->max <= 1.0f && d->step >= 1.0f) {
        snprintf(buf, n, "%s", !v.valid ? FMT_UNKNOWN : (v.value > 0.5f ? "On" : "Off"));
    } else {
        fmt_value(buf, n, v, 0, d->unit);
    }
}

static void confirm_cb(bool ok, void *user)
{
    (void)user;
    if (!app_state_lock(100)) {
        return;
    }
    ctrl_ctx_t *c = app_state_ctrl();
    const int64_t now = app_now_ms();
    if (!ok) {
        ctrl_cancel(c, now);
    } else {
        const im_value_t cur = ctrl_current_value(app_state_model(), c->active.param);
        const bool link_ok = link_allows_control(app_state_link(), now);
        const ctrl_result_t r = ctrl_confirm(c, cur, link_ok, now);
        if (r == CTRL_OK) {
            /* No write transport exists for the configured backend. A
             * transport must post the command here and report outcomes via
             * ctrl_on_accepted / ctrl_on_rejected / ctrl_on_transport_error. */
            ctrl_on_rejected(c, c->active.id, now);
        }
    }
    app_state_unlock();
}

static void value_entered(const char *text, void *user)
{
    const ctrl_param_id_t id = (ctrl_param_id_t)(intptr_t)user;
    char *end = NULL;
    const float v = strtof(text, &end);
    if (end == text) {
        ui_info("Invalid", "Not a number.");
        return;
    }
    ctrl_result_t r = CTRL_ERR_BAD_STATE;
    char cur_txt[24] = "";
    if (app_state_lock(100)) {
        const int64_t now = app_now_ms();
        const im_value_t cur = ctrl_current_value(app_state_model(), id);
        fmt_param(cur_txt, sizeof(cur_txt), ctrl_param(id), cur);
        r = ctrl_propose(app_state_ctrl(), id, cur, v, link_allows_control(app_state_link(), now), now);
        app_state_unlock();
    }
    if (r != CTRL_OK) {
        ui_info("Not allowed", ctrl_result_text(r));
        return;
    }
    const ctrl_param_desc_t *d = ctrl_param(id);
    char msg[200];
    snprintf(msg, sizeof(msg), "%s\nCurrent: %s\nNew: %.0f %s\n%s.\nThe result is reported only after read-back.",
             d->label, cur_txt, (double)v, d->unit, ctrl_class_text(d->cls));
    ui_confirm("Change inverter setting?", msg, "Apply", confirm_cb, NULL);
}

static void edit_cb(lv_event_t *e)
{
    const ctrl_param_id_t id = (ctrl_param_id_t)(intptr_t)lv_event_get_user_data(e);
    ui_text_input(ctrl_param(id)->label, NULL, false, true, 6, value_entered, (void *)(intptr_t)id);
}

static void ack_cb(lv_event_t *e)
{
    (void)e;
    if (app_state_lock(100)) {
        ctrl_acknowledge(app_state_ctrl());
        app_state_unlock();
    }
}

static void create(lv_obj_t *page)
{
    ui_flex_col(page, UI_PAD);
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);

    lv_obj_t *banner = ui_card(page, NULL, UI_CONTENT_W - 2 * UI_PAD, LV_SIZE_CONTENT);
    lv_obj_set_style_border_color(banner, g_pal.warn, 0);
    ui_label(banner, &lv_font_montserrat_20, g_pal.warn, LV_SYMBOL_WARNING " Inverter writes are disabled");
    ui_note(banner, "The configured backend exposes telemetry only (no write API), and no parameter has been "
                    "verified against the inverter firmware. Settings below are shown read-only. See "
                    "docs/writable-parameters.md for the enablement procedure.");

    lv_obj_t *tou =
        ui_card(page, "Time of Use schedule (read-only, from telemetry)", UI_CONTENT_W - 2 * UI_PAD, LV_SIZE_CONTENT);
    s_tou_days = ui_label(tou, &lv_font_montserrat_16, g_pal.text, "");
    s_tou_table = lv_table_create(tou);
    lv_table_set_column_count(s_tou_table, 6);
    static const char *const hdr[6] = {"Slot", "Start", "Power", "Batt. V", "SOC", "Charge from"};
    static const int32_t cw[6] = {80, 100, 120, 120, 100, 280};
    for (int c = 0; c < 6; c++) {
        lv_table_set_cell_value(s_tou_table, 0, (uint32_t)c, hdr[c]);
        lv_table_set_column_width(s_tou_table, (uint32_t)c, cw[c]);
    }

    lv_obj_t *params = ui_card(page, "Parameters", UI_CONTENT_W - 2 * UI_PAD, LV_SIZE_CONTENT);
    for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
        const ctrl_param_desc_t *d = ctrl_param((ctrl_param_id_t)i);
        lv_obj_t *row = lv_obj_create(params);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        ui_flex_row(row, 12);
        lv_obj_t *info = lv_obj_create(row);
        lv_obj_remove_style_all(info);
        lv_obj_set_size(info, 560, LV_SIZE_CONTENT);
        ui_flex_col(info, 2);
        ui_label(info, &lv_font_montserrat_20, g_pal.text, d->label);
        lv_obj_t *meta = ui_label(info, &lv_font_montserrat_14, g_pal.muted, "");
        if (d->max <= 1.0f && d->step >= 1.0f) {
            lv_label_set_text_fmt(meta, "On/Off  |  %s", ctrl_class_text(d->cls));
        } else {
            lv_label_set_text_fmt(meta, "Range %.0f-%.0f %s, step %.0f  |  %s", (double)d->min, (double)d->max, d->unit,
                                  (double)d->step, ctrl_class_text(d->cls));
        }
        s_rows_status[i] = ui_label(info, &lv_font_montserrat_14, g_pal.warn, "");
        s_rows_cur[i] = ui_label(row, &lv_font_montserrat_24, g_pal.text, "--");
        lv_obj_set_width(s_rows_cur[i], 110);
        s_rows_btn[i] = ui_button(row, LV_SYMBOL_EDIT " Change", edit_cb, (void *)(intptr_t)i);
    }

    lv_obj_t *cmd = ui_card(page, "Last command", UI_CONTENT_W - 2 * UI_PAD, LV_SIZE_CONTENT);
    s_cmd_lbl = ui_label(cmd, &lv_font_montserrat_16, g_pal.text, "None");
    s_ack_btn = ui_button(cmd, "Acknowledge", ack_cb, NULL);
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const im_tou_t *t = &s->model.tou;
    static const char *const days[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    char dl[96];
    size_t off = (size_t)snprintf(
        dl, sizeof(dl),
        "ToU %s.  Days: ", !t->enabled.valid ? "state unknown" : (t->enabled.value > 0.5f ? "enabled" : "disabled"));
    for (int i = 0; i < 7 && off < sizeof(dl); i++) {
        if (t->day_known_mask & (1u << i)) {
            off += (size_t)snprintf(dl + off, sizeof(dl) - off, "%s%s ", days[i],
                                    (t->day_mask & (1u << i)) ? "" : "(off)");
        }
    }
    lv_label_set_text(s_tou_days, dl);

    uint32_t row = 1;
    char buf[48];
    for (int i = 0; i < IM_TOU_SLOTS; i++) {
        const im_tou_slot_t *sl = &t->slots[i];
        if (!sl->present) {
            continue;
        }
        lv_table_set_row_count(s_tou_table, row + 1);
        lv_table_set_cell_value_fmt(s_tou_table, row, 0, "%d", i + 1);
        fmt_hhmm(buf, sizeof(buf), sl->start_minutes);
        lv_table_set_cell_value(s_tou_table, row, 1, buf);
        fmt_power(buf, sizeof(buf), sl->power_w, false);
        lv_table_set_cell_value(s_tou_table, row, 2, buf);
        fmt_value(buf, sizeof(buf), sl->voltage_v, 1, "V");
        lv_table_set_cell_value(s_tou_table, row, 3, buf);
        fmt_value(buf, sizeof(buf), sl->soc_pct, 0, "%");
        lv_table_set_cell_value(s_tou_table, row, 4, buf);
        snprintf(buf, sizeof(buf), "Grid: %s  Gen: %s",
                 !sl->grid_charge.valid ? "?" : (sl->grid_charge.value > 0.5f ? "on" : "off"),
                 !sl->gen_charge.valid ? "?" : (sl->gen_charge.value > 0.5f ? "on" : "off"));
        lv_table_set_cell_value(s_tou_table, row, 5, buf);
        row++;
    }
    if (row == 1) {
        lv_table_set_row_count(s_tou_table, 2);
        lv_table_set_cell_value(s_tou_table, 1, 0, "-");
        lv_table_set_cell_value(s_tou_table, 1, 1, "No ToU data");
    }

    ctrl_result_t can[CTRL_PARAM_COUNT];
    if (app_state_lock(0)) {
        for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
            can[i] = ctrl_can_edit(app_state_ctrl(), (ctrl_param_id_t)i);
        }
        app_state_unlock();
    } else {
        return;
    }
    const bool link_ok = link_allows_control(&s->link, s->now_ms);
    for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
        const ctrl_param_desc_t *d = ctrl_param((ctrl_param_id_t)i);
        fmt_param(buf, sizeof(buf), d, ctrl_current_value(&s->model, (ctrl_param_id_t)i));
        lv_label_set_text(s_rows_cur[i], buf);
        ui_value_style(s_rows_cur[i], ui_is_live(s));
        const bool editable = can[i] == CTRL_OK && link_ok;
        lv_label_set_text_fmt(s_rows_status[i], "%s %s", editable ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE,
                              can[i] != CTRL_OK ? ctrl_result_text(can[i])
                                                : (link_ok ? "Editable" : "Telemetry not live"));
        ui_set_enabled(s_rows_btn[i], editable);
    }

    const ctrl_command_t *a = &s->ctrl_active;
    if (a->state == CMD_IDLE) {
        lv_label_set_text_fmt(s_cmd_lbl, "No command pending. Audit log entries: %u", s->ctrl_audit_used);
        lv_obj_add_flag(s_ack_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text_fmt(s_cmd_lbl, "#%lu %s -> %.0f: %s", (unsigned long)a->id, ctrl_param(a->param)->label,
                              (double)a->requested, ctrl_state_text(a->state));
        const bool finished =
            a->state == CMD_APPLIED || a->state == CMD_FAILED || a->state == CMD_UNCERTAIN || a->state == CMD_CANCELLED;
        if (finished) {
            lv_obj_remove_flag(s_ack_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ack_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

const ui_screen_t scr_controls = {T_CONTROLS, LV_SYMBOL_EDIT, true, create, update};
