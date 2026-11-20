/* Device diagnostics readouts. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char fw_version[32];
    char idf_version[32];
    char build_date[24];
    char partition[16];
    uint64_t uptime_s;
    uint32_t heap_internal_free;
    uint32_t heap_internal_min;
    uint32_t psram_free;
    uint32_t psram_total;
    const char *reset_reason;
} diag_info_t;

void diag_read(diag_info_t *out);

#ifdef __cplusplus
}
#endif
