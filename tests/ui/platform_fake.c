/* Deterministic host platform. No radio, flash, OTA or inverter writes. */
#include "app_state.h"
#include "board.h"
#include "diag.h"
#include "net_wifi.h"
#include "ota.h"
#include "telemetry.h"
#include <stdlib.h>
#include <string.h>

static app_settings_t settings;
static app_snapshot_t snapshot;
static history_t history;
static ctrl_ctx_t control;

size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = 0;
    }
    return len;
}
const char *esp_err_to_name(esp_err_t error)
{
    (void)error;
    return "host";
}
void esp_restart(void)
{
    abort();
}
void app_state_init(const app_settings_t *s)
{
    settings = *s;
    const link_config_t cfg = {30000, 60000, 2000, 60000};
    link_init(&snapshot.link, &cfg);
    hist_init(&history);
    ctrl_init(&control, NULL);
}
int64_t app_now_ms(void)
{
    return lv_tick_get();
}
bool app_state_lock(uint32_t timeout)
{
    (void)timeout;
    return true;
}
void app_state_unlock(void)
{
}
inverter_model_t *app_state_model(void)
{
    return &snapshot.model;
}
link_state_t *app_state_link(void)
{
    return &snapshot.link;
}
wifi_info_t *app_state_wifi(void)
{
    return &snapshot.wifi;
}
history_t *app_state_history(void)
{
    return &history;
}
ctrl_ctx_t *app_state_ctrl(void)
{
    return &control;
}
void app_settings_get(app_settings_t *out)
{
    *out = settings;
}
void app_settings_set(const app_settings_t *in)
{
    settings = *in;
}
bool app_state_snapshot(app_snapshot_t *out)
{
    *out = snapshot;
    out->now_ms = app_now_ms();
    out->model_valid = snapshot.model.battery.soc_pct.valid;
    out->backend_configured = settings.backend_host[0] != 0;
    out->settings_locked = settings.pin[0] != 0;
    out->ctrl_active = control.active;
    return true;
}
esp_err_t settings_save(const app_settings_t *s)
{
    settings = *s;
    return ESP_OK;
}
esp_err_t settings_erase(void)
{
    memset(&settings, 0, sizeof(settings));
    return ESP_OK;
}
void net_wifi_reconnect(void)
{
}
void net_wifi_scan(void)
{
}
void telemetry_poll_now(void)
{
}
esp_err_t board_backlight_set(bool on)
{
    (void)on;
    return ESP_OK;
}
esp_err_t ota_start(const char *url)
{
    (void)url;
    return ESP_ERR_INVALID_STATE;
}
bool ota_url_allowed(const char *url)
{
    return strncmp(url, "https://", 8) == 0;
}
void diag_read(diag_info_t *out)
{
    memset(out, 0, sizeof(*out));
    strlcpy(out->fw_version, "host simulation", sizeof(out->fw_version));
    out->reset_reason = "Host";
}
