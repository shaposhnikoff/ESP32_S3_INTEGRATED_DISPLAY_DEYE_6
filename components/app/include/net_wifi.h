/* Wi-Fi station management. All calls are non-blocking. */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t net_wifi_start(void);
/* Re-reads settings and reconnects (call after credentials change). */
void net_wifi_reconnect(void);
/* Starts an asynchronous scan; results appear in app_state wifi info. */
void net_wifi_scan(void);

#ifdef __cplusplus
}
#endif
