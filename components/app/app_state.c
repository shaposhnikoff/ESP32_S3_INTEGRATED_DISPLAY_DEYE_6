#include "app_state.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "state";

static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_settings_lock;
static inverter_model_t s_model;
static bool s_model_valid;
static link_state_t s_link;
static wifi_info_t s_wifi;
static ota_info_t s_ota;
static history_t *s_history; /* ~68 KB, allocated in PSRAM */
static ctrl_ctx_t s_ctrl;
static uint32_t s_seq;
static char s_error[64];
static app_settings_t s_settings;
static outage_schedule_t s_schedule;

int64_t app_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void app_state_init(const app_settings_t *settings)
{
    s_lock = xSemaphoreCreateMutex();
    s_settings_lock = xSemaphoreCreateMutex();
    configASSERT(s_lock && s_settings_lock);
    s_settings = *settings;
    memset(&s_schedule, 0, sizeof(s_schedule));
    im_reset(&s_model);
    const link_config_t lc = {
        .stale_after_ms = (uint32_t)settings->stale_after_s * 1000u,
        .offline_after_ms = (uint32_t)settings->stale_after_s * 2000u,
        .backoff_min_ms = 2000,
        .backoff_max_ms = 60000,
    };
    link_init(&s_link, &lc);
    s_history = heap_caps_malloc(sizeof(history_t), MALLOC_CAP_SPIRAM);
    if (s_history == NULL) {
        ESP_LOGE(TAG, "history allocation failed; charts disabled");
    } else {
        hist_init(s_history);
    }
    /* The backend exposes no write API: every control stays disabled. */
    const ctrl_capabilities_t caps = {.backend_write_api = false};
    ctrl_init(&s_ctrl, &caps);
    s_wifi.status = settings->wifi_ssid[0] ? WIFI_ST_CONNECTING : WIFI_ST_UNCONFIGURED;
}

bool app_state_lock(uint32_t timeout_ms)
{
    return xSemaphoreTake(s_lock, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_lock);
}

inverter_model_t *app_state_model(void)
{
    return &s_model;
}
link_state_t *app_state_link(void)
{
    return &s_link;
}
wifi_info_t *app_state_wifi(void)
{
    return &s_wifi;
}
ota_info_t *app_state_ota(void)
{
    return &s_ota;
}
history_t *app_state_history(void)
{
    return s_history;
}
ctrl_ctx_t *app_state_ctrl(void)
{
    return &s_ctrl;
}
void app_state_set_model_valid(bool v)
{
    s_model_valid = v;
}
void app_state_bump_seq(void)
{
    s_seq++;
}
void app_state_set_error(const char *msg)
{
    strlcpy(s_error, msg ? msg : "", sizeof(s_error));
}

void app_settings_get(app_settings_t *out)
{
    xSemaphoreTake(s_settings_lock, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_settings_lock);
}

void app_settings_set(const app_settings_t *in)
{
    xSemaphoreTake(s_settings_lock, portMAX_DELAY);
    s_settings = *in;
    xSemaphoreGive(s_settings_lock);
    if (app_state_lock(portMAX_DELAY)) {
        s_link.cfg.stale_after_ms = (uint32_t)in->stale_after_s * 1000u;
        s_link.cfg.offline_after_ms = (uint32_t)in->stale_after_s * 2000u;
        app_state_unlock();
    }
}

bool app_state_snapshot(app_snapshot_t *out)
{
    if (xSemaphoreTake(s_settings_lock, 0) != pdTRUE) {
        return false;
    }
    out->backend_configured = settings_backend_configured(&s_settings);
    out->settings_locked = s_settings.pin[0] != '\0';
    xSemaphoreGive(s_settings_lock);

    if (!app_state_lock(0)) {
        return false;
    }
    out->model = s_model;
    out->model_valid = s_model_valid;
    out->link = s_link;
    out->wifi = s_wifi;
    out->ota = s_ota;
    out->ctrl_active = s_ctrl.active;
    out->ctrl_audit_used = s_ctrl.audit.used;
    out->telemetry_seq = s_seq;
    strlcpy(out->last_error, s_error, sizeof(out->last_error));
    out->now_ms = app_now_ms();
    app_state_unlock();
    return true;
}

void app_state_schedule_publish(const outage_schedule_t *schedule)
{
    if (app_state_lock(portMAX_DELAY)) {
        s_schedule = *schedule;
        app_state_unlock();
    }
}

bool app_state_schedule_get_day(int year, int month, int day, outage_day_t *out, int64_t *updated_epoch)
{
    if (!app_state_lock(0))
        return false;
    bool found = false;
    for (int i = 0; i < 2; i++) {
        const outage_dated_day_t *entry = &s_schedule.days[i];
        if (entry->present && entry->year == year && entry->month == month && entry->day == day) {
            *out = entry->schedule;
            *updated_epoch = s_schedule.updated_epoch;
            found = true;
            break;
        }
    }
    app_state_unlock();
    return found;
}
