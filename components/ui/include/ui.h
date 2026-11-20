/* Deye HMI user interface (LVGL 9). */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Builds every screen once and starts the refresh timer. Must be called
 * with the LVGL adapter lock held. */
void ui_start(lv_display_t *disp);
/* Starts read-only LAN diagnostics. Call after net_wifi_start(). */
esp_err_t ui_capture_server_start(void);

#ifdef __cplusplus
}
#endif
