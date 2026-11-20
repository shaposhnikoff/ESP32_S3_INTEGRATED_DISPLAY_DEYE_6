/*
 * Deye HMI entry point.
 *
 * Boot order: settings (NVS) -> shared state -> display/touch/LVGL -> UI ->
 * Wi-Fi -> telemetry. No control command is ever issued at boot and nothing
 * pending is replayed: the control context starts empty on every boot.
 */
#include "app_state.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "net_wifi.h"
#include "ota.h"
#include "sdkconfig.h"
#include "settings.h"
#include "telemetry.h"
#include "ui.h"

static const char *TAG = "main";

#define APP_VALID_AFTER_US (60LL * 1000 * 1000)

static void mark_valid_cb(void *arg)
{
    (void)arg;
    /* UI and tasks survived a minute without a crash/watchdog reset. */
    ota_mark_app_valid();
}

void app_main(void)
{
    app_settings_t settings;
    esp_err_t err = settings_load(&settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "settings load failed (%s), using defaults", esp_err_to_name(err));
        settings_defaults(&settings);
    }
    err = settings_apply_network_profile(&settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "network profile not applied (%s)", esp_err_to_name(err));
    }
    app_state_init(&settings);

    lv_display_t *disp = NULL;
    ESP_ERROR_CHECK(board_display_start(&disp));
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_start(disp);
        esp_lv_adapter_unlock();
    }

    ESP_ERROR_CHECK(net_wifi_start());
#if CONFIG_DEYE_SCREENSHOT_HTTP
    err = ui_capture_server_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "capture server not started (%s)", esp_err_to_name(err));
    }
#endif
    ESP_ERROR_CHECK(telemetry_start());

    const esp_timer_create_args_t ta = {.callback = mark_valid_cb, .name = "app_valid"};
    esp_timer_handle_t t;
    ESP_ERROR_CHECK(esp_timer_create(&ta, &t));
    ESP_ERROR_CHECK(esp_timer_start_once(t, APP_VALID_AFTER_US));
    ESP_LOGI(TAG, "Deye HMI started");
}
