/* Persistent device-side settings (NVS namespace "hmi"). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GRID_SIGN_UNVERIFIED = 0, /* show magnitude only, no direction */
    GRID_SIGN_POSITIVE_IMPORT,
    GRID_SIGN_POSITIVE_EXPORT,
} grid_sign_t;

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT,
} theme_t;

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65]; /* never displayed or logged */
    char static_ip[16]; /* empty = DHCP */
    char netmask[16];
    char gateway[16];
    char dns_server[16];
    char ntp_server[64];
    char backend_host[64];
    uint16_t backend_port;
    uint16_t poll_interval_s;
    uint16_t stale_after_s;
    uint16_t screen_timeout_s;    /* 0 = never */
    uint16_t battery_capacity_wh; /* nominal combined capacity, 0 = unknown */
    uint8_t battery_cutoff_pct;   /* reserve below which runtime is zero */
    uint8_t battery_warning_pct;  /* Overview warning threshold */
    char outage_group[8];         /* DTEK group, e.g. GPV4.1 */
    uint8_t theme;
    uint8_t grid_sign;
    uint8_t language;
    char ota_url[160];
    char pin[9]; /* optional settings PIN, empty = no lock */
} app_settings_t;

/* Loads settings (defaults for missing keys). Initialises NVS if needed. */
esp_err_t settings_load(app_settings_t *out);
esp_err_t settings_save(const app_settings_t *s);
/* Erases the "hmi" namespace (factory reset of display settings). */
esp_err_t settings_erase(void);
void settings_defaults(app_settings_t *s);
bool settings_backend_configured(const app_settings_t *s);
/* Applies a changed build-time network profile once and saves it to NVS. */
esp_err_t settings_apply_network_profile(app_settings_t *s);

#ifdef __cplusplus
}
#endif
