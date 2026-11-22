#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef struct {
    uint32_t attempts;
    uint32_t successes;
    int stage;
    int error;
    int tls_error;
    int tls_flags;
    int http_status;
    uint32_t body_bytes;
} outage_fetch_diag_t;

/* Starts a bounded background HTTPS reader for the public outage mirror. */
esp_err_t outage_fetch_start(void);
void outage_fetch_diagnostics(outage_fetch_diag_t *out);
