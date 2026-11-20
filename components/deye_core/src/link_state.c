#include "link_state.h"

#include <string.h>

void link_init(link_state_t *ls, const link_config_t *cfg)
{
    memset(ls, 0, sizeof(*ls));
    ls->cfg = *cfg;
}

void link_on_success(link_state_t *ls, int64_t now_ms, uint32_t latency_ms, bool backend_poll_ok,
                     bool backend_age_valid, float backend_age_s)
{
    ls->has_data = true;
    ls->last_ok_ms = now_ms;
    ls->backend_age_valid = backend_age_valid;
    ls->backend_age_s_at_ok = backend_age_s;
    ls->backend_poll_ok = backend_poll_ok;
    ls->consecutive_failures = 0;
    ls->total_successes++;
    ls->last_latency_ms = latency_ms;
    ls->last_error = 0;
}

void link_on_failure(link_state_t *ls, int64_t now_ms, int error)
{
    (void)now_ms;
    ls->consecutive_failures++;
    ls->total_failures++;
    ls->last_error = error;
}

int64_t link_data_age_ms(const link_state_t *ls, int64_t now_ms)
{
    if (!ls->has_data || !ls->backend_age_valid) {
        return -1;
    }
    int64_t local = now_ms - ls->last_ok_ms;
    if (local < 0) {
        local = 0;
    }
    return local + (int64_t)(ls->backend_age_s_at_ok * 1000.0f);
}

link_freshness_t link_freshness(const link_state_t *ls, int64_t now_ms)
{
    if (!ls->has_data) {
        return LINK_NEVER;
    }
    if (now_ms - ls->last_ok_ms > (int64_t)ls->cfg.offline_after_ms) {
        return LINK_OFFLINE;
    }
    if (!ls->backend_poll_ok || !ls->backend_age_valid ||
        link_data_age_ms(ls, now_ms) > (int64_t)ls->cfg.stale_after_ms) {
        return LINK_STALE;
    }
    return LINK_FRESH;
}

uint32_t link_next_delay_ms(const link_state_t *ls, uint32_t interval_ms, float jitter)
{
    if (ls->consecutive_failures == 0) {
        return interval_ms;
    }
    uint64_t d = ls->cfg.backoff_min_ms;
    for (uint32_t i = 1; i < ls->consecutive_failures && d < ls->cfg.backoff_max_ms; i++) {
        d *= 2;
    }
    if (d > ls->cfg.backoff_max_ms) {
        d = ls->cfg.backoff_max_ms;
    }
    if (jitter > 0.0f && jitter < 1.0f) {
        d += (uint64_t)((float)d * 0.2f * jitter);
    }
    return (uint32_t)d;
}

bool link_allows_control(const link_state_t *ls, int64_t now_ms)
{
    return link_freshness(ls, now_ms) == LINK_FRESH && ls->backend_poll_ok;
}

const char *link_freshness_name(link_freshness_t f)
{
    switch (f) {
    case LINK_FRESH:
        return "Live";
    case LINK_STALE:
        return "Stale";
    case LINK_OFFLINE:
        return "Offline";
    default:
        return "Waiting";
    }
}
