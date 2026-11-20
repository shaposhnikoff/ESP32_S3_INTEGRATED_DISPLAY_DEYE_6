/* Wi-Fi selection and backend endpoint configuration. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmt.h"
#include "net_wifi.h"
#include "telemetry.h"
#include "ui_internal.h"

static lv_obj_t *s_wifi_status, *s_wifi_ssid, *s_wifi_ip, *s_wifi_rssi, *s_wifi_pass;
static lv_obj_t *s_scan_btn, *s_list;
static lv_obj_t *s_host, *s_port, *s_poll;
static lv_obj_t *s_api_status, *s_api_latency, *s_api_err;
static uint32_t s_scan_seq_shown = UINT32_MAX;
static char s_pending_ssid[33];

static const uint16_t k_polls[] = {2, 3, 5, 10, 30};

static void password_entered(const char *pass, void *user)
{
    (void)user;
    app_settings_t s;
    app_settings_get(&s);
    strlcpy(s.wifi_ssid, s_pending_ssid, sizeof(s.wifi_ssid));
    strlcpy(s.wifi_pass, pass, sizeof(s.wifi_pass));
    ui_save_settings(&s, true, false);
    memset(&s, 0, sizeof(s));
}

static void open_confirmed(bool ok, void *user)
{
    (void)user;
    if (ok) {
        password_entered("", NULL);
    }
}

static void ap_cb(lv_event_t *e)
{
    const wifi_ap_info_t *ap = lv_event_get_user_data(e);
    strlcpy(s_pending_ssid, ap->ssid, sizeof(s_pending_ssid));
    if (ap->secure) {
        char title[64];
        snprintf(title, sizeof(title), "Password for %s", ap->ssid);
        ui_text_input(title, NULL, true, false, 64, password_entered, NULL);
    } else {
        ui_confirm("Open network", "This network is not encrypted. Connect anyway?", "Connect", open_confirmed, NULL);
    }
}

static void scan_cb(lv_event_t *e)
{
    (void)e;
    net_wifi_scan();
}

static void host_entered(const char *text, void *user)
{
    (void)user;
    app_settings_t s;
    app_settings_get(&s);
    strlcpy(s.backend_host, text, sizeof(s.backend_host));
    ui_save_settings(&s, false, true);
}

static void port_entered(const char *text, void *user)
{
    (void)user;
    const long p = strtol(text, NULL, 10);
    if (p < 1 || p > 65535) {
        ui_info("Invalid port", "Port must be 1-65535.");
        return;
    }
    app_settings_t s;
    app_settings_get(&s);
    s.backend_port = (uint16_t)p;
    ui_save_settings(&s, false, true);
}

static void host_cb(lv_event_t *e)
{
    (void)e;
    app_settings_t s;
    app_settings_get(&s);
    ui_text_input("Backend host or IP", s.backend_host, false, false, 63, host_entered, NULL);
}

static void port_cb(lv_event_t *e)
{
    (void)e;
    app_settings_t s;
    app_settings_get(&s);
    char p[8];
    snprintf(p, sizeof(p), "%u", s.backend_port);
    ui_text_input("Backend port", p, false, true, 5, port_entered, NULL);
}

static void poll_cb(lv_event_t *e)
{
    const uint32_t sel = lv_dropdown_get_selected(lv_event_get_target_obj(e));
    if (sel >= sizeof(k_polls) / sizeof(k_polls[0])) {
        return;
    }
    app_settings_t s;
    app_settings_get(&s);
    s.poll_interval_s = k_polls[sel];
    ui_save_settings(&s, false, true);
}

static void test_cb(lv_event_t *e)
{
    (void)e;
    telemetry_poll_now();
}

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    const int32_t w = (UI_CONTENT_W - 3 * UI_PAD) / 2;
    const int32_t h = UI_CONTENT_H - 2 * UI_PAD;

    lv_obj_t *wc = ui_card(page, LV_SYMBOL_WIFI " Wi-Fi", w, h);
    s_wifi_status = ui_kv(wc, "Status");
    s_wifi_ssid = ui_kv(wc, "Network");
    s_wifi_pass = ui_kv(wc, "Password");
    s_wifi_ip = ui_kv(wc, "IP address");
    s_wifi_rssi = ui_kv(wc, "Signal");
    s_scan_btn = ui_button(wc, LV_SYMBOL_REFRESH " Scan networks", scan_cb, NULL);
    s_list = lv_list_create(wc);
    lv_obj_set_size(s_list, LV_PCT(100), 200);

    lv_obj_t *bc = ui_card(page, LV_SYMBOL_GPS " Backend (Raspberry Pi exporter)", w, h);
    s_host = ui_kv(bc, "Host");
    s_port = ui_kv(bc, "Port");
    lv_obj_t *row = lv_obj_create(bc);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    ui_flex_row(row, 8);
    ui_button(row, "Edit host", host_cb, NULL);
    ui_button(row, "Edit port", port_cb, NULL);

    lv_obj_t *prow = lv_obj_create(bc);
    lv_obj_remove_style_all(prow);
    lv_obj_set_size(prow, LV_PCT(100), LV_SIZE_CONTENT);
    ui_flex_row(prow, 8);
    ui_label(prow, &lv_font_montserrat_16, g_pal.muted, "Poll every");
    s_poll = lv_dropdown_create(prow);
    lv_dropdown_set_options(s_poll, "2 s\n3 s\n5 s\n10 s\n30 s");
    lv_obj_add_event_cb(s_poll, poll_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_api_status = ui_kv(bc, "Telemetry");
    s_api_latency = ui_kv(bc, "Latency");
    s_api_err = ui_kv(bc, "Last error");
    ui_button(bc, LV_SYMBOL_REFRESH " Test connection", test_cb, NULL);
    ui_note(bc, "Read-only: the display only performs HTTP GET /metrics. Plain HTTP on the local network; "
                "the exporter offers no TLS or authentication.");
}

static void update(const app_snapshot_t *s, bool changed)
{
    (void)changed;
    static const char *const st[] = {"Not configured", "Connecting", "Connected", "Disconnected"};
    lv_label_set_text(s_wifi_status, st[s->wifi.status]);
    app_settings_t cfg;
    app_settings_get(&cfg);
    lv_label_set_text(s_wifi_ssid, cfg.wifi_ssid[0] ? cfg.wifi_ssid : "--");
    lv_label_set_text(s_wifi_pass, cfg.wifi_pass[0] ? "stored (hidden)" : "none");
    lv_label_set_text(s_wifi_ip, s->wifi.ip[0] ? s->wifi.ip : "--");
    if (s->wifi.status == WIFI_ST_CONNECTED) {
        lv_label_set_text_fmt(s_wifi_rssi, "%d dBm", s->wifi.rssi);
    } else {
        lv_label_set_text(s_wifi_rssi, "--");
    }
    ui_set_enabled(s_scan_btn, !s->wifi.scanning);

    if (s->wifi.scan_seq != s_scan_seq_shown) {
        s_scan_seq_shown = s->wifi.scan_seq;
        lv_obj_clean(s_list);
        /* user data must outlive the button: keep a static copy of the results */
        static wifi_ap_info_t aps[APP_SCAN_MAX];
        memcpy(aps, s->wifi.scan, sizeof(aps));
        if (s->wifi.scan_count == 0) {
            lv_list_add_text(s_list, s->wifi.scan_seq ? "No networks found" : "Tap Scan to list networks");
        }
        for (int i = 0; i < s->wifi.scan_count; i++) {
            char t[64];
            snprintf(t, sizeof(t), "%s  (%d dBm)%s", aps[i].ssid, aps[i].rssi, aps[i].secure ? "" : "  OPEN");
            lv_obj_t *b = lv_list_add_button(s_list, aps[i].secure ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING, t);
            lv_obj_add_event_cb(b, ap_cb, LV_EVENT_CLICKED, &aps[i]);
        }
    }

    lv_label_set_text(s_host, cfg.backend_host[0] ? cfg.backend_host : "(not set)");
    lv_label_set_text_fmt(s_port, "%u", cfg.backend_port);
    for (uint32_t i = 0; i < sizeof(k_polls) / sizeof(k_polls[0]); i++) {
        if (k_polls[i] == cfg.poll_interval_s && lv_dropdown_get_selected(s_poll) != i) {
            lv_dropdown_set_selected(s_poll, i);
        }
    }
    lv_label_set_text(s_api_status, link_freshness_name(link_freshness(&s->link, s->now_ms)));
    if (s->link.total_successes > 0) {
        lv_label_set_text_fmt(s_api_latency, "%lu ms", (unsigned long)s->link.last_latency_ms);
    } else {
        lv_label_set_text(s_api_latency, "--");
    }
    lv_label_set_text(s_api_err, s->last_error[0] ? s->last_error : "none");
    memset(&cfg, 0, sizeof(cfg));
}

const ui_screen_t scr_network = {T_NETWORK, LV_SYMBOL_WIFI, true, create, update};
