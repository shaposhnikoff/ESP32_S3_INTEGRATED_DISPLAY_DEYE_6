/*
 * Connection health and data freshness, independent of any transport.
 * All times are monotonic milliseconds supplied by the caller.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_NEVER = 0, /* no successful fetch since boot */
    LINK_FRESH,     /* data current */
    LINK_STALE,     /* we have data but it is older than the threshold */
    LINK_OFFLINE,   /* backend unreachable for longer than offline threshold */
} link_freshness_t;

typedef struct {
    uint32_t stale_after_ms;   /* data age beyond which values are "stale" */
    uint32_t offline_after_ms; /* time without successful fetch => offline */
    uint32_t backoff_min_ms;
    uint32_t backoff_max_ms;
} link_config_t;

typedef struct {
    link_config_t cfg;
    bool has_data;
    int64_t last_ok_ms;        /* local time of last successful fetch */
    float backend_age_s_at_ok; /* exporter's own data age at that moment */
    bool backend_age_valid;
    bool backend_poll_ok; /* exporter's last Modbus poll succeeded */
    uint32_t consecutive_failures;
    uint32_t total_failures;
    uint32_t total_successes;
    uint32_t last_latency_ms;
    int last_error; /* transport-specific code, 0 = none */
} link_state_t;

void link_init(link_state_t *ls, const link_config_t *cfg);
void link_on_success(link_state_t *ls, int64_t now_ms, uint32_t latency_ms, bool backend_poll_ok,
                     bool backend_age_valid, float backend_age_s);
void link_on_failure(link_state_t *ls, int64_t now_ms, int error);

/* Age of the displayed data: local time since fetch + exporter data age. */
int64_t link_data_age_ms(const link_state_t *ls, int64_t now_ms);
link_freshness_t link_freshness(const link_state_t *ls, int64_t now_ms);

/*
 * Delay before the next poll. With no failures this is `interval_ms`;
 * after failures it grows exponentially and is capped. `jitter` in [0,1)
 * spreads retries (pass 0 for deterministic behaviour).
 */
uint32_t link_next_delay_ms(const link_state_t *ls, uint32_t interval_ms, float jitter);

/* Controls may only be offered when data is fresh and the inverter link is ok. */
bool link_allows_control(const link_state_t *ls, int64_t now_ms);

const char *link_freshness_name(link_freshness_t f);

#ifdef __cplusplus
}
#endif
