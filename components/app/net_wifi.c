#include "net_wifi.h"

#include <string.h>

#include "app_state.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "wifi";

static esp_timer_handle_t s_retry_timer;
static uint32_t s_attempt;
static bool s_started;
static bool s_time_started;
static char s_ntp_server[64];

static void set_status(wifi_status_t st)
{
    if (app_state_lock(portMAX_DELAY)) {
        app_state_wifi()->status = st;
        if (st != WIFI_ST_CONNECTED) {
            app_state_wifi()->ip[0] = '\0';
        }
        app_state_unlock();
    }
}

static void apply_config_and_connect(void)
{
    app_settings_t s;
    app_settings_get(&s);
    if (s.wifi_ssid[0] == '\0') {
        set_status(WIFI_ST_UNCONFIGURED);
        return;
    }
    wifi_config_t wc = {0};
    memcpy(wc.sta.ssid, s.wifi_ssid, strlen(s.wifi_ssid));
    memcpy(wc.sta.password, s.wifi_pass, strlen(s.wifi_pass));
    /* Reject open networks when a password is set (no silent downgrade). */
    wc.sta.threshold.authmode = s.wifi_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wc.sta.pmf_cfg.capable = true;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    memset(&s, 0, sizeof(s));
    set_status(WIFI_ST_CONNECTING);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect: %s", esp_err_to_name(err));
    }
}

static void retry_cb(void *arg)
{
    (void)arg;
    apply_config_and_connect();
}

static void schedule_retry(void)
{
    uint32_t delay_ms = 2000u << (s_attempt < 5 ? s_attempt : 5); /* 2 s .. 64 s */
    s_attempt++;
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)delay_ms * 1000u);
    ESP_LOGI(TAG, "reconnect in %u ms", (unsigned)delay_ms);
}

static void on_scan_done(void)
{
    uint16_t n = APP_SCAN_MAX;
    wifi_ap_record_t recs[APP_SCAN_MAX];
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) {
        n = 0;
    }
    if (app_state_lock(portMAX_DELAY)) {
        wifi_info_t *w = app_state_wifi();
        w->scan_count = 0;
        for (uint16_t i = 0; i < n; i++) {
            if (recs[i].ssid[0] == '\0') {
                continue;
            }
            wifi_ap_info_t *ap = &w->scan[w->scan_count++];
            strlcpy(ap->ssid, (const char *)recs[i].ssid, sizeof(ap->ssid));
            ap->rssi = recs[i].rssi;
            ap->secure = recs[i].authmode != WIFI_AUTH_OPEN;
        }
        w->scanning = false;
        w->scan_seq++;
        app_state_unlock();
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            apply_config_and_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *d = data;
            ESP_LOGW(TAG, "disconnected, reason %d", d ? d->reason : -1);
            set_status(WIFI_ST_DISCONNECTED);
            if (app_state_lock(portMAX_DELAY)) {
                app_state_wifi()->reconnects++;
                app_state_unlock();
            }
            schedule_retry();
            break;
        }
        case WIFI_EVENT_SCAN_DONE:
            on_scan_done();
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ev = data;
        s_attempt = 0;
        esp_timer_stop(s_retry_timer);
        if (!s_time_started) {
            const esp_sntp_config_t time_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(s_ntp_server);
            s_time_started = esp_netif_sntp_init(&time_cfg) == ESP_OK;
        }
        wifi_ap_record_t ap;
        int8_t rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
        if (app_state_lock(portMAX_DELAY)) {
            wifi_info_t *w = app_state_wifi();
            w->status = WIFI_ST_CONNECTED;
            snprintf(w->ip, sizeof(w->ip), IPSTR, IP2STR(&ev->ip_info.ip));
            w->rssi = rssi;
            app_state_unlock();
        }
        ESP_LOGI(TAG, "connected, ip " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

static void rssi_cb(void *arg)
{
    (void)arg;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK && app_state_lock(100)) {
        app_state_wifi()->rssi = ap.rssi;
        app_state_unlock();
    }
}

static esp_err_t configure_station(esp_netif_t *station)
{
    app_settings_t settings;
    app_settings_get(&settings);
    strlcpy(s_ntp_server, settings.ntp_server, sizeof(s_ntp_server));
    if (settings.static_ip[0] == '\0') {
        return ESP_OK;
    }
    esp_netif_ip_info_t ip = {0};
    esp_netif_dns_info_t dns = {.ip.type = ESP_IPADDR_TYPE_V4};
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(settings.static_ip, &ip.ip), TAG, "invalid static IP");
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(settings.netmask, &ip.netmask), TAG, "invalid netmask");
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(settings.gateway, &ip.gw), TAG, "invalid gateway");
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(settings.dns_server, &dns.ip.u_addr.ip4), TAG, "invalid DNS");
    esp_err_t err = esp_netif_dhcpc_stop(station);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(station, &ip), TAG, "static IP");
    return esp_netif_set_dns_info(station, ESP_NETIF_DNS_MAIN, &dns);
}

esp_err_t net_wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *station = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); /* credentials live in our NVS namespace */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    const esp_timer_create_args_t ta = {.callback = retry_cb, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_retry_timer));
    const esp_timer_create_args_t ra = {.callback = rssi_cb, .name = "wifi_rssi"};
    esp_timer_handle_t rssi_timer;
    ESP_ERROR_CHECK(esp_timer_create(&ra, &rssi_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(rssi_timer, 10 * 1000 * 1000));
    ESP_ERROR_CHECK(configure_station(station));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_started = true;
    return ESP_OK;
}

void net_wifi_reconnect(void)
{
    if (!s_started) {
        return;
    }
    s_attempt = 0;
    esp_wifi_disconnect();
    /* Single deferred connect with the new config; a DISCONNECTED event in
     * between only re-arms the same one-shot timer. */
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, 500 * 1000);
}

void net_wifi_scan(void)
{
    if (!s_started) {
        return;
    }
    if (app_state_lock(portMAX_DELAY)) {
        app_state_wifi()->scanning = true;
        app_state_unlock();
    }
    const wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
        if (app_state_lock(portMAX_DELAY)) {
            app_state_wifi()->scanning = false;
            app_state_unlock();
        }
    }
}
