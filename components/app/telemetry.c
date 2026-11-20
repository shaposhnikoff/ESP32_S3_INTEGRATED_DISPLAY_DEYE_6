#include "telemetry.h"

#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "telemetry";

#define HTTP_TIMEOUT_MS 5000
#define MAX_BODY CONFIG_DEYE_TELEMETRY_MAX_BYTES

static TaskHandle_t s_task;
static char *s_body;              /* PSRAM receive buffer */
static inverter_model_t s_parsed; /* parsed outside the state lock */

typedef enum {
    FETCH_OK = 0,
    FETCH_ERR_CONNECT = -1,
    FETCH_ERR_HTTP = -2,
    FETCH_ERR_TOO_LARGE = -3,
    FETCH_ERR_READ = -4,
    FETCH_ERR_INVALID = -5,
} fetch_err_t;

static const char *fetch_err_text(int e)
{
    switch (e) {
    case FETCH_ERR_CONNECT:
        return "Backend unreachable";
    case FETCH_ERR_HTTP:
        return "Backend HTTP error";
    case FETCH_ERR_TOO_LARGE:
        return "Response too large";
    case FETCH_ERR_READ:
        return "Read error / timeout";
    case FETCH_ERR_INVALID:
        return "Telemetry incomplete or invalid";
    default:
        return "";
    }
}

static int fetch(const app_settings_t *s, size_t *out_len, int *http_status)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%u/metrics", s->backend_host, s->backend_port);
    const esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
        .disable_auto_redirect = true,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        return FETCH_ERR_CONNECT;
    }
    int ret = FETCH_OK;
    if (esp_http_client_open(c, 0) != ESP_OK) {
        ret = FETCH_ERR_CONNECT;
        goto out;
    }
    int64_t content_len = esp_http_client_fetch_headers(c);
    *http_status = esp_http_client_get_status_code(c);
    if (*http_status != 200) {
        ret = FETCH_ERR_HTTP;
        goto out;
    }
    if (content_len >= MAX_BODY) {
        ret = FETCH_ERR_TOO_LARGE;
        goto out;
    }
    size_t total = 0;
    for (;;) {
        if (total >= MAX_BODY - 1) {
            ret = FETCH_ERR_TOO_LARGE;
            goto out;
        }
        int n = esp_http_client_read(c, s_body + total, (int)(MAX_BODY - 1 - total));
        if (n < 0) {
            ret = FETCH_ERR_READ;
            goto out;
        }
        if (n == 0) {
            if (esp_http_client_is_complete_data_received(c)) {
                break;
            }
            ret = FETCH_ERR_READ;
            goto out;
        }
        total += (size_t)n;
    }
    s_body[total] = '\0';
    *out_len = total;
out:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ret;
}

static void push_history(const inverter_model_t *m, int64_t now_ms)
{
    history_t *h = app_state_history();
    if (h == NULL) {
        return;
    }
    float v[HIST_SERIES_COUNT] = {0};
    uint8_t mask = 0;
    const im_value_t src[HIST_SERIES_COUNT] = {
        [HIST_PV] = m->pv.total_power_w,     [HIST_LOAD] = m->load.power_w,   [HIST_GRID] = m->grid.power_w,
        [HIST_BATTERY] = m->battery.power_w, [HIST_SOC] = m->battery.soc_pct,
    };
    for (int i = 0; i < HIST_SERIES_COUNT; i++) {
        if (src[i].valid) {
            v[i] = src[i].value;
            mask |= (uint8_t)(1u << i);
        }
    }
    hist_add(h, now_ms / 1000, v, mask);
}

static void telemetry_task(void *arg)
{
    (void)arg;
    for (;;) {
        app_settings_t s;
        app_settings_get(&s);
        bool wifi_up = false;
        if (app_state_lock(portMAX_DELAY)) {
            wifi_up = app_state_wifi()->status == WIFI_ST_CONNECTED;
            app_state_unlock();
        }

        uint32_t delay_ms = (uint32_t)s.poll_interval_s * 1000u;
        if (wifi_up && settings_backend_configured(&s)) {
            size_t len = 0;
            int status = 0;
            const int64_t t0 = app_now_ms();
            int rc = fetch(&s, &len, &status);
            const int64_t t1 = app_now_ms();
            bool core_ok = false;
            if (rc == FETCH_OK) {
                core_ok = im_parse_metrics(s_body, len, &s_parsed);
                if (!core_ok) {
                    rc = FETCH_ERR_INVALID;
                }
            }
            if (app_state_lock(portMAX_DELAY)) {
                link_state_t *ls = app_state_link();
                if (rc == FETCH_OK) {
                    *app_state_model() = s_parsed;
                    app_state_set_model_valid(true);
                    link_on_success(ls, t1, (uint32_t)(t1 - t0),
                                    s_parsed.meta.scrape_success.valid && s_parsed.meta.scrape_success.value > 0.5f,
                                    s_parsed.meta.data_age_s.valid, s_parsed.meta.data_age_s.value);
                    if (ls->total_successes == 1) {
                        ESP_LOGI(TAG, "first telemetry: SOC=%.0f%% load=%.0fW state=%s",
                                 (double)s_parsed.battery.soc_pct.value, (double)s_parsed.load.power_w.value,
                                 link_freshness_name(link_freshness(ls, t1)));
                    }
                    if (link_freshness(ls, t1) == LINK_FRESH) {
                        push_history(&s_parsed, t1);
                        ctrl_on_telemetry(app_state_ctrl(), &s_parsed, t1);
                    }
                    app_state_set_error("");
                } else {
                    /* Keep the last good model; the UI marks it stale by age. */
                    link_on_failure(ls, t1, rc);
                    char msg[64];
                    if (rc == FETCH_ERR_HTTP) {
                        snprintf(msg, sizeof(msg), "Backend HTTP %d", status);
                    } else {
                        strlcpy(msg, fetch_err_text(rc), sizeof(msg));
                    }
                    app_state_set_error(msg);
                }
                app_state_bump_seq();
                delay_ms = link_next_delay_ms(ls, delay_ms, (float)(esp_random() % 1000) / 1000.0f);
                app_state_unlock();
            }
            if (rc != FETCH_OK) {
                ESP_LOGW(TAG, "poll failed: %s (next in %u ms)", fetch_err_text(rc), (unsigned)delay_ms);
            }
        }
        /* Every blocking call above is bounded by HTTP timeouts; wait for the
         * next poll or an explicit wake-up (settings change, "Test"). */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t telemetry_start(void)
{
    s_body = heap_caps_malloc(MAX_BODY, MALLOC_CAP_SPIRAM);
    if (s_body == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(telemetry_task, "telemetry", 8192, NULL, 4, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void telemetry_poll_now(void)
{
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}
