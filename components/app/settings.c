#include "settings.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *NS = "hmi";

void settings_defaults(app_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    strlcpy(s->backend_host, CONFIG_DEYE_DEFAULT_BACKEND_HOST, sizeof(s->backend_host));
    s->backend_port = CONFIG_DEYE_DEFAULT_BACKEND_PORT;
    s->poll_interval_s = 3;
    s->stale_after_s = 30;
    s->screen_timeout_s = 300;
    s->theme = THEME_DARK;
    s->grid_sign = GRID_SIGN_UNVERIFIED;
    strlcpy(s->ntp_server, "pool.ntp.org", sizeof(s->ntp_server));
}

bool settings_backend_configured(const app_settings_t *s)
{
    return s->backend_host[0] != '\0' && s->backend_port != 0;
}

static void get_str(nvs_handle_t h, const char *key, char *dst, size_t len)
{
    size_t n = len;
    if (nvs_get_str(h, key, dst, &n) != ESP_OK) {
        /* keep default */
    }
    dst[len - 1] = '\0';
}

static void get_u16(nvs_handle_t h, const char *key, uint16_t *dst, uint16_t min, uint16_t max)
{
    uint16_t v;
    if (nvs_get_u16(h, key, &v) == ESP_OK && v >= min && v <= max) {
        *dst = v;
    }
}

static void get_u8(nvs_handle_t h, const char *key, uint8_t *dst, uint8_t max)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK && v <= max) {
        *dst = v;
    }
}

esp_err_t settings_load(app_settings_t *out)
{
    settings_defaults(out);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS layout changed, erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }
    nvs_handle_t h;
    err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; /* first boot */
    }
    if (err != ESP_OK) {
        return err;
    }
    get_str(h, "ssid", out->wifi_ssid, sizeof(out->wifi_ssid));
    get_str(h, "pass", out->wifi_pass, sizeof(out->wifi_pass));
    get_str(h, "ip", out->static_ip, sizeof(out->static_ip));
    get_str(h, "netmask", out->netmask, sizeof(out->netmask));
    get_str(h, "gateway", out->gateway, sizeof(out->gateway));
    get_str(h, "dns", out->dns_server, sizeof(out->dns_server));
    get_str(h, "ntp", out->ntp_server, sizeof(out->ntp_server));
    get_str(h, "host", out->backend_host, sizeof(out->backend_host));
    get_u16(h, "port", &out->backend_port, 1, 65535);
    get_u16(h, "poll", &out->poll_interval_s, 2, 60);
    get_u16(h, "stale", &out->stale_after_s, 10, 3600);
    get_u16(h, "scr_to", &out->screen_timeout_s, 0, 3600);
    get_u8(h, "theme", &out->theme, THEME_LIGHT);
    get_u8(h, "gsign", &out->grid_sign, GRID_SIGN_POSITIVE_EXPORT);
    get_u8(h, "lang", &out->language, 0);
    get_str(h, "ota_url", out->ota_url, sizeof(out->ota_url));
    get_str(h, "pin", out->pin, sizeof(out->pin));
    nvs_close(h);
    ESP_LOGI(TAG, "loaded: wifi=%s backend=%s:%u poll=%us", out->wifi_ssid[0] ? "configured" : "unset",
             out->backend_host[0] ? out->backend_host : "(unset)", out->backend_port, out->poll_interval_s);
    return ESP_OK;
}

esp_err_t settings_save(const app_settings_t *s)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
#define TRY(x)                                                                                                         \
    do {                                                                                                               \
        if ((err = (x)) != ESP_OK) {                                                                                   \
            nvs_close(h);                                                                                              \
            return err;                                                                                                \
        }                                                                                                              \
    } while (0)
    TRY(nvs_set_str(h, "ssid", s->wifi_ssid));
    TRY(nvs_set_str(h, "pass", s->wifi_pass));
    TRY(nvs_set_str(h, "ip", s->static_ip));
    TRY(nvs_set_str(h, "netmask", s->netmask));
    TRY(nvs_set_str(h, "gateway", s->gateway));
    TRY(nvs_set_str(h, "dns", s->dns_server));
    TRY(nvs_set_str(h, "ntp", s->ntp_server));
    TRY(nvs_set_str(h, "host", s->backend_host));
    TRY(nvs_set_u16(h, "port", s->backend_port));
    TRY(nvs_set_u16(h, "poll", s->poll_interval_s));
    TRY(nvs_set_u16(h, "stale", s->stale_after_s));
    TRY(nvs_set_u16(h, "scr_to", s->screen_timeout_s));
    TRY(nvs_set_u8(h, "theme", s->theme));
    TRY(nvs_set_u8(h, "gsign", s->grid_sign));
    TRY(nvs_set_u8(h, "lang", s->language));
    TRY(nvs_set_str(h, "ota_url", s->ota_url));
    TRY(nvs_set_str(h, "pin", s->pin));
#undef TRY
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t settings_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
