/* Background telemetry polling of the exporter (read-only HTTP GET). */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t telemetry_start(void);
/* Wakes the poller immediately (settings changed / "Test connection"). */
void telemetry_poll_now(void);

#ifdef __cplusplus
}
#endif
