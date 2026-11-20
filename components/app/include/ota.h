/* Firmware update over HTTPS (HTTP only with CONFIG_DEYE_OTA_ALLOW_HTTP). */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Validates the URL scheme and starts the update task. */
esp_err_t ota_start(const char *url);
bool ota_url_allowed(const char *url);
/* Marks the running image valid (cancels rollback) once the app is healthy. */
void ota_mark_app_valid(void);

#ifdef __cplusplus
}
#endif
