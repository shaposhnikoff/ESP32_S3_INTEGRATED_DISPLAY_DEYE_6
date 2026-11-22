idf_component_register(
    SRCS "settings.c" "network_profile.c" "app_state.c" "net_wifi.c" "telemetry.c" "outage_fetch.c" "ota.c" "diag.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../config"
    REQUIRES deye_core
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_timer esp_http_client esp_https_ota mbedtls
                  app_update mbedtls esp_app_format)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
