/* Build-time provisioning. A profile is imported only when its digest changes. */
#include "settings.h"

#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

#if __has_include("network.local.h")
#include "network.local.h"
#else
#include "network.example.h"
#endif

_Static_assert(sizeof(DEYE_WIFI_SSID) <= 33, "Wi-Fi SSID exceeds 32 bytes");
_Static_assert(sizeof(DEYE_WIFI_PASSWORD) <= 65, "Wi-Fi password exceeds 64 bytes");
_Static_assert(sizeof(DEYE_BACKEND_HOST) <= 64, "Backend host exceeds 63 bytes");
_Static_assert(DEYE_BACKEND_PORT > 0 && DEYE_BACKEND_PORT <= 65535, "Invalid backend port");
_Static_assert(DEYE_POLL_INTERVAL_S >= 2 && DEYE_POLL_INTERVAL_S <= 60, "Invalid polling interval");
_Static_assert(DEYE_STALE_AFTER_S >= 10 && DEYE_STALE_AFTER_S <= 3600, "Invalid stale threshold");
_Static_assert(sizeof(DEYE_STATIC_IP) <= 16 && sizeof(DEYE_NETMASK) <= 16 && sizeof(DEYE_GATEWAY) <= 16 &&
                   sizeof(DEYE_DNS_SERVER) <= 16,
               "IPv4 address exceeds 15 bytes");
_Static_assert(sizeof(DEYE_NTP_SERVER) > 1 && sizeof(DEYE_NTP_SERVER) <= 64, "Invalid NTP server length");

static const char *TAG = "network_profile";

static const app_settings_t profile = {
    .wifi_ssid = DEYE_WIFI_SSID,
    .wifi_pass = DEYE_WIFI_PASSWORD,
    .backend_host = DEYE_BACKEND_HOST,
    .backend_port = DEYE_BACKEND_PORT,
    .poll_interval_s = DEYE_POLL_INTERVAL_S,
    .stale_after_s = DEYE_STALE_AFTER_S,
    .static_ip = DEYE_STATIC_IP,
    .netmask = DEYE_NETMASK,
    .gateway = DEYE_GATEWAY,
    .dns_server = DEYE_DNS_SERVER,
    .ntp_server = DEYE_NTP_SERVER,
};

static bool profile_valid(void)
{
    const size_t password_len = strlen(profile.wifi_pass);
    if (password_len != 0 &&
        (password_len < 8 || (password_len == 64 && strspn(profile.wifi_pass, "0123456789abcdefABCDEF") != 64))) {
        return false;
    }
    if (strcspn(profile.backend_host, "/:@?#[] \t\r\n") != strlen(profile.backend_host)) {
        return false;
    }
    const char *addresses[] = {profile.static_ip, profile.netmask, profile.gateway, profile.dns_server};
    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++) {
        if (profile.static_ip[0] == '\0') {
            if (addresses[i][0] != '\0') {
                return false;
            }
        } else {
            esp_ip4_addr_t address;
            if (esp_netif_str_to_ip4(addresses[i], &address) != ESP_OK || address.addr == 0) {
                return false;
            }
        }
    }
    return true;
}

esp_err_t settings_apply_network_profile(app_settings_t *s)
{
    if (profile.wifi_ssid[0] == '\0') {
        return ESP_OK; /* Empty template does not replace existing NVS settings. */
    }
    if (!profile_valid()) {
        ESP_LOGE(TAG, "invalid local network profile; keeping saved settings");
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t digest[32];
    if (mbedtls_sha256((const unsigned char *)&profile, sizeof(profile), digest, 0) != 0) {
        return ESP_FAIL;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("hmi", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t previous[sizeof(digest)];
    size_t len = sizeof(previous);
    const bool unchanged = nvs_get_blob(h, "net_profile", previous, &len) == ESP_OK && len == sizeof(digest) &&
                           memcmp(previous, digest, sizeof(digest)) == 0;
    nvs_close(h);
    if (unchanged) {
        return ESP_OK;
    }
    app_settings_t updated = *s;
#define COPY(field) memcpy(updated.field, profile.field, sizeof(updated.field))
    COPY(wifi_ssid);
    COPY(wifi_pass);
    COPY(backend_host);
    COPY(static_ip);
    COPY(netmask);
    COPY(gateway);
    COPY(dns_server);
    COPY(ntp_server);
#undef COPY
    updated.backend_port = profile.backend_port;
    updated.poll_interval_s = profile.poll_interval_s;
    updated.stale_after_s = profile.stale_after_s;
    err = settings_save(&updated);
    if (err == ESP_OK) {
        *s = updated;
        err = nvs_open("hmi", NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_blob(h, "net_profile", digest, sizeof(digest));
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
    }
    memset(&updated, 0, sizeof(updated));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "local network profile saved; automatic Wi-Fi connection enabled");
    }
    return err;
}
