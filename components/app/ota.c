#include "ota.h"

#include <string.h>
#include <time.h>

#include "app_state.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "ota";
static char s_url[160];

static void set_ota(ota_status_t st, int pct, const char *msg)
{
    if (app_state_lock(portMAX_DELAY)) {
        ota_info_t *o = app_state_ota();
        o->status = st;
        o->progress_pct = pct;
        strlcpy(o->message, msg, sizeof(o->message));
        app_state_unlock();
    }
}

bool ota_url_allowed(const char *url)
{
    if (strncmp(url, "https://", 8) == 0) {
        return true;
    }
#if CONFIG_DEYE_OTA_ALLOW_HTTP
    return strncmp(url, "http://", 7) == 0;
#else
    return false;
#endif
}

static void ota_task(void *arg)
{
    (void)arg;
    /* A cold boot has no wall clock; certificate dates cannot be checked
     * until SNTP has supplied the time. Wait only in the OTA worker. */
    if (strncmp(s_url, "https://", 8) == 0 && time(NULL) < 1704067200) {
        set_ota(OTA_ST_RUNNING, 0, "Synchronizing clock");
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
            set_ota(OTA_ST_FAILED, -1, "Clock unavailable; check NTP access");
            vTaskDelete(NULL);
            return;
        }
    }
    esp_http_client_config_t http = {
        .url = s_url,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t cfg = {.http_config = &http};
    esp_https_ota_handle_t h = NULL;
    esp_err_t err = esp_https_ota_begin(&cfg, &h);
    if (err != ESP_OK) {
        set_ota(OTA_ST_FAILED, -1, "Cannot start update");
        ESP_LOGE(TAG, "begin: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    const int total = esp_https_ota_get_image_size(h);
    while ((err = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        const int read = esp_https_ota_get_image_len_read(h);
        set_ota(OTA_ST_RUNNING, total > 0 ? (int)((int64_t)read * 100 / total) : -1, "Downloading");
    }
    if (err == ESP_OK && esp_https_ota_is_complete_data_received(h)) {
        err = esp_https_ota_finish(h);
        if (err == ESP_OK) {
            set_ota(OTA_ST_DONE, 100, "Installed, restarting");
            ESP_LOGI(TAG, "update installed, restarting");
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_restart();
        }
        set_ota(OTA_ST_FAILED, -1, err == ESP_ERR_OTA_VALIDATE_FAILED ? "Image invalid" : "Finish failed");
    } else {
        esp_https_ota_abort(h);
        set_ota(OTA_ST_FAILED, -1, "Download failed");
    }
    ESP_LOGE(TAG, "update failed: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
}

esp_err_t ota_start(const char *url)
{
    if (url == NULL || !ota_url_allowed(url)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool busy = false;
    if (app_state_lock(portMAX_DELAY)) {
        busy = app_state_ota()->status == OTA_ST_RUNNING;
        app_state_unlock();
    }
    if (busy) {
        return ESP_ERR_INVALID_STATE;
    }
    strlcpy(s_url, url, sizeof(s_url));
    set_ota(OTA_ST_RUNNING, 0, "Connecting");
    if (xTaskCreate(ota_task, "ota", 8192, NULL, 3, NULL) != pdPASS) {
        set_ota(OTA_ST_FAILED, -1, "No memory");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void ota_mark_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "image marked valid");
    }
}
