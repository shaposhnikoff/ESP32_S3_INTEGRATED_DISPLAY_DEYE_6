#include "outage_fetch.h"

#include <stdio.h>
#include <time.h>

#include "app_state.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OUTAGE_URL "https://raw.githubusercontent.com/Baskerville42/outage-data-ua/main/data/kyiv-region.json"
#define OUTAGE_BODY_MAX (128 * 1024)
#define OUTAGE_POLL_MS (10 * 60 * 1000)

static const char *TAG = "outage_fetch";
static char *s_body;
static volatile outage_fetch_diag_t s_diag;

void outage_fetch_diagnostics(outage_fetch_diag_t *out)
{
    out->attempts = s_diag.attempts;
    out->successes = s_diag.successes;
    out->stage = s_diag.stage;
    out->error = s_diag.error;
    out->tls_error = s_diag.tls_error;
    out->tls_flags = s_diag.tls_flags;
    out->http_status = s_diag.http_status;
    out->body_bytes = s_diag.body_bytes;
}

static bool fetch(size_t *length)
{
    const esp_http_client_config_t config = {
        .url = OUTAGE_URL,
        .timeout_ms = 10000,
        .method = HTTP_METHOD_GET,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        s_diag.stage = 2;
        s_diag.error = ESP_ERR_NO_MEM;
        return false;
    }
    bool ok = false;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        s_diag.stage = 3;
        s_diag.error = err;
        int tls_error = 0;
        int tls_flags = 0;
        esp_http_client_get_and_clear_last_tls_error(client, &tls_error, &tls_flags);
        s_diag.tls_error = tls_error;
        s_diag.tls_flags = tls_flags;
        goto done;
    }
    int64_t content_length = esp_http_client_fetch_headers(client);
    s_diag.http_status = esp_http_client_get_status_code(client);
    if (s_diag.http_status != 200 || content_length >= OUTAGE_BODY_MAX) {
        s_diag.stage = 4;
        goto done;
    }
    size_t used = 0;
    for (;;) {
        if (used >= OUTAGE_BODY_MAX - 1) {
            s_diag.stage = 5;
            goto done;
        }
        int read = esp_http_client_read(client, s_body + used, OUTAGE_BODY_MAX - 1 - used);
        if (read < 0) {
            s_diag.stage = 6;
            goto done;
        }
        if (read == 0) {
            if (!esp_http_client_is_complete_data_received(client)) {
                s_diag.stage = 7;
                goto done;
            }
            break;
        }
        used += (size_t)read;
    }
    s_body[used] = 0;
    *length = used;
    s_diag.body_bytes = used;
    ok = true;
done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok;
}

static void outage_task(void *arg)
{
    (void)arg;
    s_diag.stage = 11;
    for (;;) {
        bool connected = false;
        if (app_state_lock(0)) {
            connected = app_state_wifi()->status == WIFI_ST_CONNECTED;
            app_state_unlock();
        }
        time_t now = time(NULL);
        if (connected && now < 1704067200)
            s_diag.stage = 12;
        if (!connected)
            s_diag.stage = 13;
        uint32_t delay_ms = 30000;
        if (connected && now >= 1704067200) {
            s_diag.attempts++;
            s_diag.stage = 1;
            s_diag.error = 0;
            s_diag.tls_error = 0;
            s_diag.tls_flags = 0;
            s_diag.http_status = 0;
            s_diag.body_bytes = 0;
            size_t length = 0;
            app_settings_t settings;
            app_settings_get(&settings);
            outage_schedule_t parsed;
            bool fetched = fetch(&length);
            bool valid = fetched && outage_parse_mirror(s_body, length, settings.outage_group, now, &parsed);
            if (valid) {
                app_state_schedule_publish(&parsed);
                s_diag.successes++;
                s_diag.stage = 9;
                ESP_LOGI(TAG, "schedule group %s updated; today=%u tomorrow=%u", settings.outage_group,
                         parsed.days[0].schedule.count, parsed.days[1].schedule.count);
                delay_ms = OUTAGE_POLL_MS;
            } else {
                if (fetched)
                    s_diag.stage = 8;
                ESP_LOGW(TAG, "schedule unavailable or invalid; retry in 30 seconds");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t outage_fetch_start(void)
{
    s_body = heap_caps_malloc(OUTAGE_BODY_MAX, MALLOC_CAP_SPIRAM);
    if (s_body == NULL) {
        s_diag.stage = -1;
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreateWithCaps(outage_task, "outage_fetch", 8192, NULL, 3, NULL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) !=
        pdPASS) {
        s_diag.stage = -2;
        heap_caps_free(s_body);
        s_body = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_diag.stage = 10;
    return ESP_OK;
}
