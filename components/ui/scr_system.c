/* Display-side settings, firmware update, restart and factory reset. */
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "ota.h"
#include "settings.h"
#include "ui_internal.h"

static lv_obj_t *s_theme, *s_timeout, *s_gsign, *s_lang;
static lv_obj_t *s_pin_lbl, *s_ota_url, *s_ota_status, *s_ota_btn;

static const uint16_t k_timeouts[] = {0, 60, 300, 900, 1800};

static void save_dd(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target_obj(e);
    const uint32_t sel = lv_dropdown_get_selected(dd);
    app_settings_t s;
    app_settings_get(&s);
    if (dd == s_theme) {
        s.theme = (uint8_t)sel;
        ui_info("Theme", "The new theme is applied after restart.");
    } else if (dd == s_timeout && sel < sizeof(k_timeouts) / sizeof(k_timeouts[0])) {
        s.screen_timeout_s = k_timeouts[sel];
    } else if (dd == s_gsign) {
        s.grid_sign = (uint8_t)sel;
    }
    ui_save_settings(&s, false, false);
}

static void restart_confirmed(bool ok, void *user)
{
    (void)user;
    if (ok) {
        esp_restart();
    }
}

static void factory_confirmed(bool ok, void *user)
{
    (void)user;
    if (!ok) {
        return;
    }
    if (settings_erase() == ESP_OK) {
        esp_restart();
    }
    ui_info("Error", "Factory reset failed.");
}

static void restart_cb(lv_event_t *e)
{
    (void)e;
    ui_confirm("Restart display?", "The display restarts. The inverter is not affected.", "Restart", restart_confirmed,
               NULL);
}

static void factory_cb(lv_event_t *e)
{
    (void)e;
    ui_confirm("Factory reset display?",
               "Erases Wi-Fi credentials, backend address, PIN and display settings, then restarts. "
               "Inverter settings are not affected.",
               "Erase", factory_confirmed, NULL);
}

static void pin_set(const char *text, void *user)
{
    (void)user;
    const size_t n = strlen(text);
    if (n != 0 && (n < 4 || n > 8)) {
        ui_info("PIN", "PIN must be 4-8 digits (empty removes it).");
        return;
    }
    app_settings_t s;
    app_settings_get(&s);
    strlcpy(s.pin, text, sizeof(s.pin));
    ui_save_settings(&s, false, false);
}

static void pin_cb(lv_event_t *e)
{
    (void)e;
    ui_text_input("New settings PIN (empty = none)", NULL, true, true, 8, pin_set, NULL);
}

static void ota_url_set(const char *text, void *user)
{
    (void)user;
    if (text[0] != '\0' && !ota_url_allowed(text)) {
        ui_info("Firmware URL", "Only https:// URLs are accepted by this build.");
        return;
    }
    app_settings_t s;
    app_settings_get(&s);
    strlcpy(s.ota_url, text, sizeof(s.ota_url));
    ui_save_settings(&s, false, false);
}

static void ota_url_cb(lv_event_t *e)
{
    (void)e;
    app_settings_t s;
    app_settings_get(&s);
    ui_text_input("Firmware image URL (https://...)", s.ota_url, false, false, sizeof(s.ota_url) - 1, ota_url_set,
                  NULL);
}

static void ota_confirmed(bool ok, void *user)
{
    (void)user;
    if (!ok) {
        return;
    }
    app_settings_t s;
    app_settings_get(&s);
    const esp_err_t err = ota_start(s.ota_url);
    if (err != ESP_OK) {
        ui_info("Update", err == ESP_ERR_INVALID_ARG ? "Invalid firmware URL." : "Update already running.");
    }
}

static void ota_cb(lv_event_t *e)
{
    (void)e;
    ui_confirm("Install firmware update?",
               "The image is downloaded and verified, then the display restarts. If the new firmware fails "
               "to start, the previous version is restored automatically.",
               "Install", ota_confirmed, NULL);
}

static lv_obj_t *dd_row(lv_obj_t *parent, const char *label, const char *opts)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_label(row, &lv_font_montserrat_16, g_pal.muted, label);
    lv_obj_t *dd = lv_dropdown_create(row);
    lv_obj_set_width(dd, 230);
    lv_dropdown_set_options(dd, opts);
    lv_obj_add_event_cb(dd, save_dd, LV_EVENT_VALUE_CHANGED, NULL);
    return dd;
}

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    const int32_t w = (UI_CONTENT_W - 3 * UI_PAD) / 2;
    const int32_t h = UI_CONTENT_H - 2 * UI_PAD;

    lv_obj_t *d = ui_card(page, "Display", w, h);
    s_theme = dd_row(d, "Theme", "Dark\nLight");
    s_timeout = dd_row(d, "Screen off after", "Never\n1 min\n5 min\n15 min\n30 min");
    s_gsign = dd_row(d, "Grid power sign", "Unverified\n+ = import\n- = import");
    s_lang = dd_row(d, "Language", "English");
    ui_note(d, "Backlight is on/off only (no dimming on this board). Grid direction stays hidden until "
               "the sign convention is confirmed against the inverter/meter. Russian/Ukrainian require "
               "a Cyrillic font build.");
    lv_obj_t *prow = lv_obj_create(d);
    lv_obj_remove_style_all(prow);
    lv_obj_set_size(prow, LV_PCT(100), LV_SIZE_CONTENT);
    ui_flex_row(prow, 8);
    ui_button(prow, LV_SYMBOL_SETTINGS " Settings PIN", pin_cb, NULL);
    s_pin_lbl = ui_label(prow, &lv_font_montserrat_16, g_pal.muted, "");

    lv_obj_t *m = ui_card(page, "Maintenance", w, h);
    ui_label(m, &lv_font_montserrat_16, g_pal.muted, "Firmware update");
    s_ota_url = ui_label(m, &lv_font_montserrat_14, g_pal.text, "");
    lv_label_set_long_mode(s_ota_url, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_ota_url, LV_PCT(100));
    lv_obj_t *orow = lv_obj_create(m);
    lv_obj_remove_style_all(orow);
    lv_obj_set_size(orow, LV_PCT(100), LV_SIZE_CONTENT);
    ui_flex_row(orow, 8);
    ui_button(orow, "Set URL", ota_url_cb, NULL);
    s_ota_btn = ui_button(orow, LV_SYMBOL_DOWNLOAD " Install", ota_cb, NULL);
    s_ota_status = ui_label(m, &lv_font_montserrat_16, g_pal.text, "");
    lv_obj_t *spacer = lv_obj_create(m);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 30);
    ui_button(m, LV_SYMBOL_REFRESH " Restart display", restart_cb, NULL);
    lv_obj_t *fr = ui_button(m, LV_SYMBOL_TRASH " Factory reset display", factory_cb, NULL);
    lv_obj_set_style_bg_color(fr, g_pal.err, 0);
}

static void update(const app_snapshot_t *s, bool changed)
{
    (void)changed;
    app_settings_t st;
    app_settings_get(&st);
    if (lv_dropdown_get_selected(s_theme) != st.theme) {
        lv_dropdown_set_selected(s_theme, st.theme);
    }
    for (uint32_t i = 0; i < sizeof(k_timeouts) / sizeof(k_timeouts[0]); i++) {
        if (k_timeouts[i] == st.screen_timeout_s && lv_dropdown_get_selected(s_timeout) != i) {
            lv_dropdown_set_selected(s_timeout, i);
        }
    }
    if (lv_dropdown_get_selected(s_gsign) != st.grid_sign) {
        lv_dropdown_set_selected(s_gsign, st.grid_sign);
    }
    (void)s_lang;
    lv_label_set_text(s_pin_lbl, st.pin[0] ? "PIN set" : "No PIN");
    lv_label_set_text(s_ota_url, st.ota_url[0] ? st.ota_url : "(no URL set)");
    const bool running = s->ota.status == OTA_ST_RUNNING;
    ui_set_enabled(s_ota_btn, st.ota_url[0] != '\0' && !running);
    switch (s->ota.status) {
    case OTA_ST_RUNNING:
        if (s->ota.progress_pct >= 0) {
            lv_label_set_text_fmt(s_ota_status, "%s %d%%", s->ota.message, s->ota.progress_pct);
        } else {
            lv_label_set_text(s_ota_status, s->ota.message);
        }
        break;
    case OTA_ST_IDLE:
        lv_label_set_text(s_ota_status, "");
        break;
    default:
        lv_label_set_text(s_ota_status, s->ota.message);
        break;
    }
    memset(&st, 0, sizeof(st));
}

const ui_screen_t scr_system = {T_SYSTEM, LV_SYMBOL_SETTINGS, true, create, update};
