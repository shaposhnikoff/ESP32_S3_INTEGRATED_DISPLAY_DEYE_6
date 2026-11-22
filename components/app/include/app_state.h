/*
 * Centralised application state.
 *
 * Owned by this module and protected by a mutex. Producers (telemetry,
 * Wi-Fi, OTA tasks) update it under the lock; the UI copies a snapshot under
 * the lock from inside the LVGL task and never blocks on it (try-lock).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "control.h"
#include "history.h"
#include "inverter_model.h"
#include "link_state.h"
#include "outage_dial.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_ST_UNCONFIGURED = 0,
    WIFI_ST_CONNECTING,
    WIFI_ST_CONNECTED,
    WIFI_ST_DISCONNECTED,
} wifi_status_t;

#define APP_SCAN_MAX 16

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;
} wifi_ap_info_t;

typedef struct {
    wifi_status_t status;
    char ip[16];
    int8_t rssi;
    uint32_t reconnects;
    bool scanning;
    uint8_t scan_count;
    wifi_ap_info_t scan[APP_SCAN_MAX];
    uint32_t scan_seq;
} wifi_info_t;

typedef enum {
    OTA_ST_IDLE = 0,
    OTA_ST_RUNNING,
    OTA_ST_DONE, /* image written, reboot pending */
    OTA_ST_FAILED,
} ota_status_t;

typedef struct {
    ota_status_t status;
    int progress_pct; /* -1 = unknown size */
    char message[64];
} ota_info_t;

/* Plain-data copy handed to the UI. */
typedef struct {
    inverter_model_t model;
    bool model_valid; /* last payload contained core telemetry */
    link_state_t link;
    wifi_info_t wifi;
    ota_info_t ota;
    ctrl_command_t ctrl_active;
    uint16_t ctrl_audit_used;
    uint32_t telemetry_seq; /* increments on every telemetry update */
    char last_error[64];
    int64_t now_ms; /* monotonic time the snapshot was taken */
    bool backend_configured;
    bool settings_locked; /* a PIN is configured */
} app_snapshot_t;

void app_state_init(const app_settings_t *settings);

/* Lock with timeout in ms (0 = try once). Returns false on timeout. */
bool app_state_lock(uint32_t timeout_ms);
void app_state_unlock(void);

/* Accessors valid only while holding the lock. */
inverter_model_t *app_state_model(void);
link_state_t *app_state_link(void);
wifi_info_t *app_state_wifi(void);
ota_info_t *app_state_ota(void);
history_t *app_state_history(void);
ctrl_ctx_t *app_state_ctrl(void);
void app_state_set_model_valid(bool v);
void app_state_bump_seq(void);
void app_state_set_error(const char *msg);

/* Settings copy (separate small lock). */
void app_settings_get(app_settings_t *out);
void app_settings_set(const app_settings_t *in);

/* Copies everything the UI needs. Non-blocking: returns false if busy. */
bool app_state_snapshot(app_snapshot_t *out);
void app_state_schedule_publish(const outage_schedule_t *schedule);
bool app_state_schedule_get_day(int year, int month, int day, outage_day_t *out, int64_t *updated_epoch);

int64_t app_now_ms(void);

#ifdef __cplusplus
}
#endif
