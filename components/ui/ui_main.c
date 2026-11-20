#include <stdio.h>
#include <string.h>

#include "board.h"
#include "esp_log.h"
#include "fmt.h"
#include "net_wifi.h"
#include "settings.h"
#include "telemetry.h"
#include "ui.h"
#include "ui_internal.h"

static const char *TAG = "ui";

#define REFRESH_MS 500
#define PIN_UNLOCK_MS (5 * 60 * 1000)

static const ui_screen_t *const k_screens[SCR_COUNT] = {
    [SCR_OVERVIEW] = &scr_overview, [SCR_BATTERY] = &scr_battery,     [SCR_SOLAR] = &scr_solar,
    [SCR_GRID] = &scr_grid,         [SCR_GENERATOR] = &scr_generator, [SCR_CHARTS] = &scr_charts,
    [SCR_CONTROLS] = &scr_controls, [SCR_NETWORK] = &scr_network,     [SCR_DIAG] = &scr_diag,
    [SCR_SYSTEM] = &scr_system,
};

static lv_obj_t *s_pages[SCR_COUNT];
static lv_obj_t *s_nav_btns[SCR_COUNT];
static ui_screen_id_t s_current = SCR_OVERVIEW;
static app_snapshot_t s_snap;
static uint32_t s_last_seq = UINT32_MAX;
static bool s_force = true;

static lv_obj_t *s_title;
static lv_obj_t *s_link_pill;
static lv_obj_t *s_link_lbl;
static lv_obj_t *s_age_lbl;
static lv_obj_t *s_wifi_lbl;
static lv_obj_t *s_alarm_lbl;

static bool s_backlight_on = true;
static lv_obj_t *s_wake_overlay;
static int64_t s_pin_unlocked_until;

/* ---------------- shared helpers ---------------- */

bool ui_is_live(const app_snapshot_t *s)
{
    return s->model_valid && link_freshness(&s->link, s->now_ms) == LINK_FRESH;
}

const char *ui_battery_state_text(im_value_t p)
{
    if (!p.valid) {
        return "Unknown";
    }
    if (p.value > 20.0f) {
        return "Discharging";
    }
    if (p.value < -20.0f) {
        return "Charging";
    }
    return "Idle";
}

bool ui_save_settings(const app_settings_t *s, bool wifi_changed, bool backend_changed)
{
    esp_err_t err = settings_save(s);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "settings save failed: %s", esp_err_to_name(err));
        ui_info("Error", "Could not save settings to flash.");
        return false;
    }
    app_settings_set(s);
    if (wifi_changed) {
        net_wifi_reconnect();
    }
    if (backend_changed) {
        telemetry_poll_now();
    }
    return true;
}

/* ---------------- navigation ---------------- */

static void show_screen(ui_screen_id_t id)
{
    for (int i = 0; i < SCR_COUNT; i++) {
        if (i == (int)id) {
            lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(s_nav_btns[i], LV_STATE_CHECKED);
        } else {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_state(s_nav_btns[i], LV_STATE_CHECKED);
        }
    }
    s_current = id;
    lv_label_set_text(s_title, tr(k_screens[id]->title));
    s_force = true;
}

typedef struct {
    ui_screen_id_t target;
} pin_req_t;

static void pin_entered(const char *text, void *user)
{
    const ui_screen_id_t target = (ui_screen_id_t)(intptr_t)user;
    app_settings_t s;
    app_settings_get(&s);
    const bool ok = s.pin[0] != '\0' && strcmp(text, s.pin) == 0;
    memset(&s, 0, sizeof(s));
    if (ok) {
        s_pin_unlocked_until = app_now_ms() + PIN_UNLOCK_MS;
        show_screen(target);
    } else {
        ui_info("Locked", "Incorrect PIN.");
    }
}

static void nav_cb(lv_event_t *e)
{
    const ui_screen_id_t id = (ui_screen_id_t)(intptr_t)lv_event_get_user_data(e);
    app_settings_t settings;
    app_settings_get(&settings);
    const bool locked = settings.pin[0] != '\0' && app_now_ms() >= s_pin_unlocked_until;
    memset(&settings, 0, sizeof(settings));
    if (k_screens[id]->protected_by_pin && locked) {
        /* keep the current tab highlighted */
        lv_obj_remove_state(s_nav_btns[id], LV_STATE_CHECKED);
        ui_text_input("Enter settings PIN", NULL, true, true, 8, pin_entered, (void *)(intptr_t)id);
        return;
    }
    show_screen(id);
}

/* ---------------- status bar ---------------- */

static void update_status_bar(const app_snapshot_t *s)
{
    const link_freshness_t f = link_freshness(&s->link, s->now_ms);
    const char *txt;
    const char *icon;
    lv_color_t col;
    if (!s->backend_configured) {
        txt = tr(T_NOT_CONFIGURED);
        icon = LV_SYMBOL_SETTINGS;
        col = g_pal.warn;
    } else if (f == LINK_FRESH && s->model_valid) {
        txt = tr(T_LIVE);
        icon = LV_SYMBOL_OK;
        col = g_pal.ok;
    } else if (f == LINK_STALE || (f == LINK_FRESH && !s->model_valid)) {
        txt = tr(T_STALE);
        icon = LV_SYMBOL_WARNING;
        col = g_pal.warn;
    } else if (f == LINK_OFFLINE) {
        txt = tr(T_OFFLINE);
        icon = LV_SYMBOL_CLOSE;
        col = g_pal.err;
    } else {
        txt = tr(T_WAITING);
        icon = LV_SYMBOL_REFRESH;
        col = g_pal.muted;
    }
    lv_label_set_text_fmt(s_link_lbl, "%s %s", icon, txt);
    lv_obj_set_style_bg_color(s_link_pill, col, 0);

    char age[16];
    fmt_age_ms(age, sizeof(age), link_data_age_ms(&s->link, s->now_ms));
    lv_label_set_text_fmt(s_age_lbl, "Data age %s", age);

    switch (s->wifi.status) {
    case WIFI_ST_CONNECTED:
        lv_label_set_text_fmt(s_wifi_lbl, LV_SYMBOL_WIFI " %d dBm", s->wifi.rssi);
        break;
    case WIFI_ST_CONNECTING:
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " connecting");
        break;
    case WIFI_ST_UNCONFIGURED:
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " not set");
        break;
    default:
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " down");
        break;
    }

    const im_run_status_t rs = s->model.inverter.run_status;
    if (s->model_valid && (rs == IM_RUN_ALARM || rs == IM_RUN_FAULT)) {
        lv_label_set_text_fmt(s_alarm_lbl, LV_SYMBOL_WARNING " INVERTER %s", rs == IM_RUN_FAULT ? "FAULT" : "ALARM");
        lv_obj_remove_flag(s_alarm_lbl, LV_OBJ_FLAG_HIDDEN);
    } else if (s->model_valid && s->model.meta.scrape_success.valid && s->model.meta.scrape_success.value < 0.5f) {
        lv_label_set_text(s_alarm_lbl, LV_SYMBOL_WARNING " EXPORTER LOST INVERTER");
        lv_obj_remove_flag(s_alarm_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_alarm_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------------- backlight timeout ---------------- */

static void wake_cb(lv_event_t *e)
{
    (void)e;
    board_backlight_set(true);
    s_backlight_on = true;
    lv_obj_delete_async(s_wake_overlay);
    s_wake_overlay = NULL;
    lv_display_trigger_activity(NULL);
}

static void handle_backlight(void)
{
    app_settings_t st;
    app_settings_get(&st);
    if (st.screen_timeout_s == 0 || !s_backlight_on) {
        return;
    }
    if (lv_display_get_inactive_time(NULL) > (uint32_t)st.screen_timeout_s * 1000u) {
        if (board_backlight_set(false) == ESP_OK) {
            s_backlight_on = false;
            /* First touch only wakes the screen; it must not press a button. */
            s_wake_overlay = lv_obj_create(lv_layer_top());
            lv_obj_remove_style_all(s_wake_overlay);
            lv_obj_set_size(s_wake_overlay, UI_W, UI_H);
            lv_obj_add_flag(s_wake_overlay, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_wake_overlay, wake_cb, LV_EVENT_PRESSED, NULL);
        }
    }
}

/* ---------------- refresh ---------------- */

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    if (!app_state_snapshot(&s_snap)) {
        return; /* producer holds the lock; try again next tick */
    }
    if (s_snap.settings_locked && k_screens[s_current]->protected_by_pin && app_now_ms() >= s_pin_unlocked_until) {
        show_screen(SCR_OVERVIEW);
    }
    static uint32_t tick;
    /* Periodic re-render so ageing data turns "stale" without new packets. */
    const bool changed = s_force || s_snap.telemetry_seq != s_last_seq || (++tick % 10) == 0;
    s_last_seq = s_snap.telemetry_seq;
    s_force = false;
    update_status_bar(&s_snap);
    k_screens[s_current]->update(&s_snap, changed);
    handle_backlight();
}

bool ui_capture_select_screen(const char *name)
{
    static const char *const names[SCR_COUNT] = {
        "overview", "battery", "solar", "grid", "generator", "charts", NULL, NULL, "diagnostics", NULL,
    };
    if (lv_obj_get_child_count(lv_layer_top()) != 0) {
        return false;
    }
    if (name != NULL) {
        bool found = false;
        for (int i = 0; i < SCR_COUNT; i++) {
            if (names[i] != NULL && strcmp(name, names[i]) == 0) {
                show_screen((ui_screen_id_t)i);
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    if (k_screens[s_current]->protected_by_pin) {
        return false;
    }
    refresh_cb(NULL);
    lv_obj_update_layout(lv_screen_active());
    return true;
}

/* ---------------- construction ---------------- */

static lv_obj_t *build_status_bar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_W, UI_STATUS_H);
    lv_obj_set_style_bg_color(bar, g_pal.card, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar, 16, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *brand = ui_label(bar, &lv_font_montserrat_20, g_pal.accent, LV_SYMBOL_CHARGE " DEYE HMI");
    lv_obj_align(brand, LV_ALIGN_LEFT_MID, 0, 0);
    s_title = ui_label(bar, &lv_font_montserrat_24, g_pal.text, "");
    lv_obj_align(s_title, LV_ALIGN_LEFT_MID, 190, 0);

    s_alarm_lbl = ui_label(bar, &lv_font_montserrat_20, g_pal.err, "");
    lv_obj_align(s_alarm_lbl, LV_ALIGN_CENTER, 60, 0);
    lv_obj_add_flag(s_alarm_lbl, LV_OBJ_FLAG_HIDDEN);

    s_link_pill = lv_obj_create(bar);
    lv_obj_remove_style_all(s_link_pill);
    lv_obj_set_size(s_link_pill, 140, 36);
    lv_obj_set_style_radius(s_link_pill, 18, 0);
    lv_obj_set_style_bg_opa(s_link_pill, LV_OPA_COVER, 0);
    lv_obj_align(s_link_pill, LV_ALIGN_RIGHT_MID, 0, 0);
    s_link_lbl = ui_label(s_link_pill, &lv_font_montserrat_16, lv_color_white(), "");
    lv_obj_center(s_link_lbl);

    s_wifi_lbl = ui_label(bar, &lv_font_montserrat_16, g_pal.muted, "");
    lv_obj_align(s_wifi_lbl, LV_ALIGN_RIGHT_MID, -156, -10);
    s_age_lbl = ui_label(bar, &lv_font_montserrat_14, g_pal.muted, "");
    lv_obj_align(s_age_lbl, LV_ALIGN_RIGHT_MID, -156, 12);
    return bar;
}

static void build_nav(lv_obj_t *scr)
{
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, UI_NAV_W, UI_CONTENT_H);
    lv_obj_set_pos(nav, 0, UI_STATUS_H);
    lv_obj_set_style_bg_color(nav, g_pal.card, 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    ui_flex_col(nav, 2);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t h = (UI_CONTENT_H - 8 - 2 * (SCR_COUNT - 1)) / SCR_COUNT;
    for (int i = 0; i < SCR_COUNT; i++) {
        lv_obj_t *b = lv_button_create(nav);
        lv_obj_set_size(b, UI_NAV_W - 8, h);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_bg_color(b, g_pal.card, 0);
        lv_obj_set_style_bg_color(b, g_pal.accent, LV_STATE_CHECKED);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, g_pal.text, 0);
        lv_label_set_text_fmt(l, "%s  %s", k_screens[i]->icon, tr(k_screens[i]->title));
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_nav_btns[i] = b;
    }
}

void ui_start(lv_display_t *disp)
{
    app_settings_t st;
    app_settings_get(&st);
    i18n_set_language((i18n_lang_t)st.language);
    ui_theme_init(disp, st.theme != THEME_LIGHT);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, g_pal.bg, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    build_status_bar(scr);
    build_nav(scr);

    for (int i = 0; i < SCR_COUNT; i++) {
        lv_obj_t *page = lv_obj_create(scr);
        lv_obj_remove_style_all(page);
        lv_obj_set_size(page, UI_CONTENT_W, UI_CONTENT_H);
        lv_obj_set_pos(page, UI_NAV_W, UI_STATUS_H);
        lv_obj_set_style_pad_all(page, UI_PAD, 0);
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        s_pages[i] = page;
        k_screens[i]->create(page);
    }
    show_screen(SCR_OVERVIEW);
    lv_timer_create(refresh_cb, REFRESH_MS, NULL);
    ESP_LOGI(TAG, "UI ready");
}
