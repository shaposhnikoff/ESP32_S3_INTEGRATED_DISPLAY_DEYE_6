#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

void ui_flex_col(lv_obj_t *obj, int32_t gap)
{
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(obj, gap, 0);
}

void ui_flex_row(lv_obj_t *obj, int32_t gap)
{
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(obj, gap, 0);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *ui_card(lv_obj_t *parent, const char *title, int32_t w, int32_t h)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, g_pal.card, 0);
    lv_obj_set_style_border_color(c, g_pal.border, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_all(c, UI_PAD, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    ui_flex_col(c, 6);
    if (title != NULL) {
        ui_label(c, &lv_font_montserrat_16, g_pal.muted, title);
    }
    return c;
}

lv_obj_t *ui_kv(lv_obj_t *parent, const char *key)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_label(row, &lv_font_montserrat_16, g_pal.muted, key);
    return ui_label(row, &lv_font_montserrat_20, g_pal.text, "--");
}

lv_obj_t *ui_note(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = ui_label(parent, &lv_font_montserrat_14, g_pal.muted, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    return l;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height(b, 52); /* touch target >= 48 px */
    lv_obj_set_style_min_width(b, 120, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (cb != NULL) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    return b;
}

void ui_set_enabled(lv_obj_t *obj, bool en)
{
    if (en) {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

void ui_value_style(lv_obj_t *label, bool live)
{
    lv_obj_set_style_text_color(label, live ? g_pal.text : g_pal.muted, 0);
}

/* ---------------- charts ---------------- */

void ui_chart_create(ui_chart_t *c, lv_obj_t *parent, int32_t w, int32_t h, uint16_t points, lv_color_t color)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_size(wrap, w, h);
    lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    c->chart = lv_chart_create(wrap);
    lv_obj_set_size(c->chart, w - 70, h);
    lv_obj_align(c->chart, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_chart_set_type(c->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c->chart, points);
    lv_chart_set_div_line_count(c->chart, 4, 6);
    lv_obj_set_style_bg_color(c->chart, g_pal.card, 0);
    lv_obj_set_style_border_color(c->chart, g_pal.border, 0);
    lv_obj_set_style_line_color(c->chart, g_pal.border, LV_PART_MAIN);
    lv_obj_set_style_size(c->chart, 0, 0, LV_PART_INDICATOR); /* no point markers */
    lv_obj_set_style_line_width(c->chart, 2, LV_PART_ITEMS);
    c->ser = lv_chart_add_series(c->chart, color, LV_CHART_AXIS_PRIMARY_Y);
    c->points = points;

    c->max_lbl = ui_label(wrap, &lv_font_montserrat_14, g_pal.muted, "");
    lv_obj_align(c->max_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    c->min_lbl = ui_label(wrap, &lv_font_montserrat_14, g_pal.muted, "");
    lv_obj_align(c->min_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    c->empty_lbl = ui_label(wrap, &lv_font_montserrat_16, g_pal.muted, "No history yet");
    lv_obj_align_to(c->empty_lbl, c->chart, LV_ALIGN_CENTER, 0, 0);
}

void ui_chart_set_color(ui_chart_t *c, lv_color_t color)
{
    lv_chart_set_series_color(c->chart, c->ser, color);
}

#define CHART_MAX_POINTS 240

bool ui_chart_fill(ui_chart_t *c, hist_range_t range, hist_series_t series, int64_t now_ms)
{
    static float vals[CHART_MAX_POINTS];
    static bool valid[CHART_MAX_POINTS];
    const uint16_t n_req = c->points > CHART_MAX_POINTS ? CHART_MAX_POINTS : c->points;
    if (!app_state_lock(0)) {
        return false;
    }
    size_t n = 0;
    const history_t *h = app_state_history();
    if (h != NULL) {
        n = hist_query(h, range, now_ms / 1000, series, vals, valid, n_req);
    }
    app_state_unlock();

    float lo = 0, hi = 0;
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        if (!valid[i]) {
            continue;
        }
        if (!any || vals[i] < lo) {
            lo = vals[i];
        }
        if (!any || vals[i] > hi) {
            hi = vals[i];
        }
        any = true;
    }
    if (series == HIST_SOC) {
        lo = 0;
        hi = 100;
    } else {
        if (lo > 0) {
            lo = 0;
        }
        if (hi - lo < 100) {
            hi = lo + 100;
        }
    }
    lv_chart_set_axis_range(c->chart, LV_CHART_AXIS_PRIMARY_Y, (int32_t)lo, (int32_t)(hi + 0.999f));
    for (uint16_t i = 0; i < c->points; i++) {
        int32_t v = LV_CHART_POINT_NONE;
        if (i < n && valid[i]) {
            v = (int32_t)vals[i];
        }
        lv_chart_set_series_value_by_id(c->chart, c->ser, i, v);
    }
    lv_chart_refresh(c->chart);
    const char *unit = series == HIST_SOC ? "%" : "W";
    lv_label_set_text_fmt(c->max_lbl, "%d %s", (int)(hi + 0.999f), unit);
    lv_label_set_text_fmt(c->min_lbl, "%d %s", (int)lo, unit);
    if (any) {
        lv_obj_add_flag(c->empty_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(c->empty_lbl, LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

/* ---------------- dialogs ---------------- */

typedef struct {
    ui_confirm_cb_t cb;
    void *user;
    lv_obj_t *mbox;
} confirm_ctx_t;

static void confirm_btn_cb(lv_event_t *e)
{
    confirm_ctx_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target_obj(e);
    const bool ok = lv_obj_get_user_data(btn) != NULL;
    ui_confirm_cb_t cb = ctx->cb;
    void *user = ctx->user;
    lv_msgbox_close(ctx->mbox);
    lv_free(ctx);
    if (cb != NULL) {
        cb(ok, user);
    }
}

void ui_confirm(const char *title, const char *text, const char *ok_text, ui_confirm_cb_t cb, void *user)
{
    confirm_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return;
    }
    ctx->cb = cb;
    ctx->user = user;
    lv_obj_t *mb = lv_msgbox_create(NULL); /* modal */
    ctx->mbox = mb;
    lv_obj_set_width(mb, 560);
    lv_msgbox_add_title(mb, title);
    lv_obj_t *t = lv_msgbox_add_text(mb, text);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_t *cancel = lv_msgbox_add_footer_button(mb, tr(T_CANCEL));
    lv_obj_t *ok = lv_msgbox_add_footer_button(mb, ok_text ? ok_text : tr(T_CONFIRM));
    lv_obj_set_user_data(cancel, NULL);
    lv_obj_set_user_data(ok, (void *)1);
    lv_obj_set_height(cancel, 52);
    lv_obj_set_height(ok, 52);
    lv_obj_set_style_bg_color(ok, g_pal.warn, 0);
    lv_obj_add_event_cb(cancel, confirm_btn_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(ok, confirm_btn_cb, LV_EVENT_CLICKED, ctx);
}

void ui_info(const char *title, const char *text)
{
    lv_obj_t *mb = lv_msgbox_create(NULL);
    lv_obj_set_width(mb, 560);
    lv_msgbox_add_title(mb, title);
    lv_msgbox_add_text(mb, text);
    lv_msgbox_add_close_button(mb);
}

typedef struct {
    ui_text_cb_t cb;
    void *user;
    lv_obj_t *overlay;
    lv_obj_t *ta;
    bool closing;
} text_ctx_t;

static void text_deleted_cb(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

static void text_done_cb(lv_event_t *e)
{
    text_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx->closing) {
        return;
    }
    ctx->closing = true;
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY && ctx->cb != NULL) {
        /* copy before the textarea is deleted */
        char buf[200];
        lv_strlcpy(buf, lv_textarea_get_text(ctx->ta), sizeof(buf));
        ui_text_cb_t cb = ctx->cb;
        void *user = ctx->user;
        lv_obj_delete_async(ctx->overlay);
        cb(buf, user);
        memset(buf, 0, sizeof(buf));
        return;
    }
    if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        lv_obj_delete_async(ctx->overlay);
    }
}

void ui_text_input(const char *title, const char *initial, bool password, bool numeric, uint32_t max_len,
                   ui_text_cb_t cb, void *user)
{
    text_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return;
    }
    ctx->cb = cb;
    ctx->user = user;
    ctx->closing = false;
    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    ctx->overlay = ov;
    lv_obj_add_event_cb(ov, text_deleted_cb, LV_EVENT_DELETE, ctx);
    lv_obj_set_size(ov, UI_W, UI_H);
    lv_obj_set_style_bg_color(ov, g_pal.bg, 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ov, 0, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = ui_label(ov, &lv_font_montserrat_24, g_pal.text, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_t *ta = lv_textarea_create(ov);
    ctx->ta = ta;
    lv_obj_set_size(ta, UI_W - 40, 64);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, password);
    if (max_len > 0) {
        lv_textarea_set_max_length(ta, max_len);
    }
    if (numeric) {
        lv_textarea_set_accepted_chars(ta, "0123456789");
    }
    if (initial != NULL && !password) {
        lv_textarea_set_text(ta, initial);
    }
    lv_obj_t *kb = lv_keyboard_create(ov);
    lv_obj_set_size(kb, UI_W - 20, 400);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_keyboard_set_mode(kb, numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, text_done_cb, LV_EVENT_READY, ctx);
    lv_obj_add_event_cb(kb, text_done_cb, LV_EVENT_CANCEL, ctx);
}
