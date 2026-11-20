/* Diagnostics and troubleshooting readouts. */
#include <stdio.h>

#include "diag.h"
#include "fmt.h"
#include "ui_internal.h"

enum { D_FW, D_IDF, D_BUILD, D_PART, D_UP, D_RESET, D_HEAP, D_HEAPMIN, D_PSRAM, D_N };
enum { N_RSSI, N_LAT, N_LAST, N_OK, N_FAIL, N_CONSEC, N_ERR, N_N };
enum { B_SERIAL, B_SCRAPE, B_AGE, B_KNOWN, B_REJ, B_MAL, B_CMDS, B_N };

static lv_obj_t *s_d[D_N], *s_n[N_N], *s_b[B_N];

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    const int32_t w = (UI_CONTENT_W - 4 * UI_PAD) / 3;
    const int32_t h = UI_CONTENT_H - 2 * UI_PAD;
    lv_obj_t *dc = ui_card(page, "Device", w, h);
    static const char *const dk[D_N] = {"Firmware",   "ESP-IDF",   "Built",    "Partition", "Uptime",
                                        "Last reset", "Free heap", "Min heap", "Free PSRAM"};
    for (int i = 0; i < D_N; i++) {
        s_d[i] = ui_kv(dc, dk[i]);
    }
    lv_obj_t *nc = ui_card(page, "Connection", w, h);
    static const char *const nk[N_N] = {"Wi-Fi RSSI",   "Latency",           "Last success", "Polls OK",
                                        "Polls failed", "Consecutive fails", "Last error"};
    for (int i = 0; i < N_N; i++) {
        s_n[i] = ui_kv(nc, nk[i]);
    }
    lv_obj_t *bc = ui_card(page, "Backend data", w, h);
    static const char *const bk[B_N] = {"Inverter S/N", "Exporter poll",   "Exporter age",   "Known metrics",
                                        "Rejected",     "Malformed lines", "Commands logged"};
    for (int i = 0; i < B_N; i++) {
        s_b[i] = ui_kv(bc, bk[i]);
    }
}

static void update(const app_snapshot_t *s, bool changed)
{
    (void)changed;
    diag_info_t d;
    diag_read(&d);
    lv_label_set_text(s_d[D_FW], d.fw_version);
    lv_label_set_text(s_d[D_IDF], d.idf_version);
    lv_label_set_text(s_d[D_BUILD], d.build_date);
    lv_label_set_text(s_d[D_PART], d.partition);
    lv_label_set_text_fmt(s_d[D_UP], "%lluh %02llum", (unsigned long long)(d.uptime_s / 3600),
                          (unsigned long long)((d.uptime_s / 60) % 60));
    lv_label_set_text(s_d[D_RESET], d.reset_reason);
    lv_label_set_text_fmt(s_d[D_HEAP], "%lu KB", (unsigned long)(d.heap_internal_free / 1024));
    lv_label_set_text_fmt(s_d[D_HEAPMIN], "%lu KB", (unsigned long)(d.heap_internal_min / 1024));
    lv_label_set_text_fmt(s_d[D_PSRAM], "%lu / %lu KB", (unsigned long)(d.psram_free / 1024),
                          (unsigned long)(d.psram_total / 1024));

    if (s->wifi.status == WIFI_ST_CONNECTED) {
        lv_label_set_text_fmt(s_n[N_RSSI], "%d dBm", s->wifi.rssi);
    } else {
        lv_label_set_text(s_n[N_RSSI], "--");
    }
    lv_label_set_text_fmt(s_n[N_LAT], "%lu ms", (unsigned long)s->link.last_latency_ms);
    char age[16];
    fmt_age_ms(age, sizeof(age), s->link.has_data ? s->now_ms - s->link.last_ok_ms : -1);
    lv_label_set_text_fmt(s_n[N_LAST], "%s ago", age);
    lv_label_set_text_fmt(s_n[N_OK], "%lu", (unsigned long)s->link.total_successes);
    lv_label_set_text_fmt(s_n[N_FAIL], "%lu", (unsigned long)s->link.total_failures);
    lv_label_set_text_fmt(s_n[N_CONSEC], "%lu", (unsigned long)s->link.consecutive_failures);
    lv_label_set_text(s_n[N_ERR], s->last_error[0] ? s->last_error : "none");

    const im_meta_t *m = &s->model.meta;
    lv_label_set_text(s_b[B_SERIAL], m->serial[0] ? m->serial : "--");
    lv_label_set_text(s_b[B_SCRAPE],
                      !m->scrape_success.valid ? "--" : (m->scrape_success.value > 0.5f ? "OK" : "FAILED"));
    char buf[24];
    fmt_value(buf, sizeof(buf), m->data_age_s, 1, "s");
    lv_label_set_text(s_b[B_AGE], buf);
    lv_label_set_text_fmt(s_b[B_KNOWN], "%u", (unsigned)m->known_samples);
    lv_label_set_text_fmt(s_b[B_REJ], "%u", (unsigned)m->rejected_samples);
    lv_label_set_text_fmt(s_b[B_MAL], "%u", (unsigned)m->malformed_lines);
    lv_label_set_text_fmt(s_b[B_CMDS], "%u", (unsigned)s->ctrl_audit_used);
}

const ui_screen_t scr_diag = {T_DIAGNOSTICS, LV_SYMBOL_EYE_OPEN, false, create, update};
