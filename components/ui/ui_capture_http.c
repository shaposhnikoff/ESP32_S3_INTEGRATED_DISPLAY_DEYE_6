/* Live device screenshots. All rendering runs under the LVGL adapter lock. */
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#if CONFIG_DEYE_SCREENSHOT_HTTP

#include "app_state.h"
#include "diag.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "outage_fetch.h"
#include "ui.h"
#include "ui_internal.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "capture";

static esp_err_t unavailable(httpd_req_t *req, const char *message)
{
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, message);
}

static void put_u32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static esp_err_t send_bmp(httpd_req_t *req, const lv_draw_buf_t *image)
{
    const uint32_t width = image->header.w;
    const uint32_t height = image->header.h;
    const uint32_t row_bytes = (width * 2 + 3) & ~3u;
    /* RGB565 BITFIELDS, top-down pixels; no second full-size conversion buffer. */
    uint8_t header[66] = {'B', 'M'};
    put_u32(header + 2, sizeof(header) + row_bytes * height);
    put_u32(header + 10, sizeof(header));
    put_u32(header + 14, 40);
    put_u32(header + 18, width);
    put_u32(header + 22, (uint32_t)-(int32_t)height);
    header[26] = 1;
    header[28] = 16;
    put_u32(header + 30, 3);
    put_u32(header + 34, row_bytes * height);
    put_u32(header + 54, 0xF800);
    put_u32(header + 58, 0x07E0);
    put_u32(header + 62, 0x001F);
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send_chunk(req, (const char *)header, sizeof(header));
    const int64_t deadline = esp_timer_get_time() + 15LL * 1000 * 1000;
    for (uint32_t y = 0; y < height && err == ESP_OK; y++) {
        if (esp_timer_get_time() > deadline) {
            return ESP_ERR_TIMEOUT;
        }
        err = httpd_resp_send_chunk(req, (const char *)image->data + y * image->header.stride, row_bytes);
    }
    return err == ESP_OK ? httpd_resp_send_chunk(req, NULL, 0) : err;
}

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    char query[64];
    char screen[24];
    const char *selected = NULL;
    if (httpd_req_get_url_query_len(req) > 0) {
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "screen", screen, sizeof(screen)) != ESP_OK) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid screen query");
        }
        selected = screen;
    }
    if (esp_lv_adapter_lock(2000) != ESP_OK) {
        return unavailable(req, "Display busy");
    }
    const bool accessible = ui_capture_select_screen(selected);
    esp_lv_adapter_unlock();
    if (!accessible) {
        return unavailable(req, "Screen unavailable");
    }
    if (selected != NULL) {
        /* The default theme delays transitions by 70 ms. Let the actual
         * display settle before capture, without blocking the LVGL task. */
        vTaskDelay(pdMS_TO_TICKS(LV_THEME_DEFAULT_TRANSITION_TIME + 70 + 2 * LV_DEF_REFR_PERIOD));
    }
    if (esp_lv_adapter_lock(2000) != ESP_OK) {
        return unavailable(req, "Display busy");
    }
    lv_draw_buf_t *image =
        ui_capture_select_screen(NULL) ? lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565) : NULL;
    esp_lv_adapter_unlock();
    if (image == NULL) {
        return unavailable(req, "Screen unavailable or insufficient memory");
    }
    /* Network transmission never holds the LVGL lock. */
    const esp_err_t err = send_bmp(req, image);
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    lv_draw_buf_destroy(image);
    esp_lv_adapter_unlock();
    return err;
}

static esp_err_t health_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"service\":\"deye-hmi\"}");
}

static esp_err_t metrics_handler(httpd_req_t *req)
{
    diag_info_t info;
    diag_read(&info);
    outage_fetch_diag_t outage;
    outage_fetch_diagnostics(&outage);
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    outage_day_t today = {0};
    int64_t schedule_updated = 0;
    bool scheduled =
        app_state_schedule_get_day(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, &today, &schedule_updated);
    int wifi_status = -1;
    if (app_state_lock(0)) {
        wifi_status = app_state_wifi()->status;
        app_state_unlock();
    }
    char body[850];
    snprintf(body, sizeof(body),
             "deye_hmi_uptime_seconds %llu\n"
             "deye_hmi_heap_free_bytes %lu\n"
             "deye_hmi_heap_min_free_bytes %lu\n"
             "deye_hmi_heap_largest_free_block_bytes %lu\n"
             "deye_hmi_psram_free_bytes %lu\n"
             "deye_hmi_psram_total_bytes %lu\n"
             "deye_hmi_epoch_seconds %lld\n"
             "deye_hmi_wifi_status %d\n"
             "deye_hmi_outage_attempts_total %lu\n"
             "deye_hmi_outage_successes_total %lu\n"
             "deye_hmi_outage_stage %d\n"
             "deye_hmi_outage_error %d\n"
             "deye_hmi_outage_tls_error %d\n"
             "deye_hmi_outage_tls_flags %d\n"
             "deye_hmi_outage_http_status %d\n"
             "deye_hmi_outage_body_bytes %lu\n"
             "deye_hmi_outage_today_available %d\n"
             "deye_hmi_outage_today_intervals %u\n",
             (unsigned long long)info.uptime_s, (unsigned long)info.heap_internal_free,
             (unsigned long)info.heap_internal_min,
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned long)info.psram_free,
             (unsigned long)info.psram_total, (long long)now, wifi_status, (unsigned long)outage.attempts,
             (unsigned long)outage.successes, outage.stage, outage.error, outage.tls_error, outage.tls_flags,
             outage.http_status, (unsigned long)outage.body_bytes, scheduled, today.count);
    httpd_resp_set_type(req, "text/plain; version=0.0.4");
    return httpd_resp_sendstr(req, body);
}

esp_err_t ui_capture_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_open_sockets = 2;
    config.lru_purge_enable = true;
    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "HTTP start");
    const httpd_uri_t handlers[] = {
        {.uri = "/screenshot.bmp", .method = HTTP_GET, .handler = screenshot_handler},
        {.uri = "/health", .method = HTTP_GET, .handler = health_handler},
        {.uri = "/metrics", .method = HTTP_GET, .handler = metrics_handler},
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(server, &handlers[i]);
        if (err != ESP_OK) {
            httpd_stop(server);
            return err;
        }
    }
    ESP_LOGI(TAG, "LAN screenshots available at /screenshot.bmp");
    return ESP_OK;
}
#endif
#endif
