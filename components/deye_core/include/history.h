/*
 * Bounded, statically sized telemetry history kept on the device.
 *
 * Two tiers: 1-minute buckets for 24 h and 15-minute buckets for 7 days.
 * Memory is fixed at compile time (~45 KB) and the history is volatile:
 * it is lost on reboot. Long-term history requires a backend time-series
 * store (see docs/api-contract.md).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HIST_PV = 0,
    HIST_LOAD,
    HIST_GRID,
    HIST_BATTERY,
    HIST_SOC,
    HIST_SERIES_COUNT,
} hist_series_t;

typedef enum {
    HIST_RANGE_1H = 0,
    HIST_RANGE_6H,
    HIST_RANGE_24H,
    HIST_RANGE_7D,
    HIST_RANGE_COUNT,
} hist_range_t;

#define HIST_FINE_STEP_S 60
#define HIST_FINE_LEN 1440
#define HIST_COARSE_STEP_S 900
#define HIST_COARSE_LEN 672

typedef struct {
    float sum[HIST_SERIES_COUNT];
    uint16_t count[HIST_SERIES_COUNT];
} hist_acc_t;

typedef struct {
    int64_t bucket; /* bucket number (time / step), -1 = empty */
    float mean[HIST_SERIES_COUNT];
    uint8_t valid_mask;
} hist_point_t;

typedef struct {
    hist_point_t *buf;
    uint16_t len;
    uint32_t step_s;
    uint16_t head; /* next write index */
    uint16_t used;
    int64_t cur_bucket;
    hist_acc_t acc;
} hist_tier_t;

typedef struct {
    hist_point_t fine_buf[HIST_FINE_LEN];
    hist_point_t coarse_buf[HIST_COARSE_LEN];
    hist_tier_t fine;
    hist_tier_t coarse;
} history_t;

void hist_init(history_t *h);

/* Adds one sample. `t_s` is a monotonic time in seconds; samples with
 * decreasing time are ignored. `valid_mask` bit i covers values[i]. */
void hist_add(history_t *h, int64_t t_s, const float values[HIST_SERIES_COUNT], uint8_t valid_mask);

/*
 * Copies the points for `range` ending at `now_s` into `out`, oldest
 * first, aggregating to at most `max_points` (mean of merged buckets).
 * Returns the number of points; points with no data are flagged invalid.
 */
size_t hist_query(const history_t *h, hist_range_t range, int64_t now_s, hist_series_t series, float *out,
                  bool *out_valid, size_t max_points);

uint32_t hist_range_seconds(hist_range_t range);

#ifdef __cplusplus
}
#endif
