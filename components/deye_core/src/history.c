#include "history.h"

#include <string.h>

static void tier_init(hist_tier_t *t, hist_point_t *buf, uint16_t len, uint32_t step)
{
    t->buf = buf;
    t->len = len;
    t->step_s = step;
    t->head = 0;
    t->used = 0;
    t->cur_bucket = -1;
    memset(&t->acc, 0, sizeof(t->acc));
    for (uint16_t i = 0; i < len; i++) {
        buf[i].bucket = -1;
        buf[i].valid_mask = 0;
    }
}

void hist_init(history_t *h)
{
    tier_init(&h->fine, h->fine_buf, HIST_FINE_LEN, HIST_FINE_STEP_S);
    tier_init(&h->coarse, h->coarse_buf, HIST_COARSE_LEN, HIST_COARSE_STEP_S);
}

static void tier_flush(hist_tier_t *t)
{
    if (t->cur_bucket < 0) {
        return;
    }
    hist_point_t *p = &t->buf[t->head];
    p->bucket = t->cur_bucket;
    p->valid_mask = 0;
    for (int s = 0; s < HIST_SERIES_COUNT; s++) {
        if (t->acc.count[s] > 0) {
            p->mean[s] = t->acc.sum[s] / (float)t->acc.count[s];
            p->valid_mask |= (uint8_t)(1u << s);
        } else {
            p->mean[s] = 0.0f;
        }
    }
    t->head = (uint16_t)((t->head + 1) % t->len);
    if (t->used < t->len) {
        t->used++;
    }
    memset(&t->acc, 0, sizeof(t->acc));
}

static void tier_add(hist_tier_t *t, int64_t t_s, const float *v, uint8_t mask)
{
    const int64_t b = t_s / (int64_t)t->step_s;
    if (t->cur_bucket >= 0 && b < t->cur_bucket) {
        return;
    }
    if (b != t->cur_bucket) {
        tier_flush(t);
        t->cur_bucket = b;
    }
    for (int s = 0; s < HIST_SERIES_COUNT; s++) {
        if ((mask & (1u << s)) && t->acc.count[s] < UINT16_MAX) {
            t->acc.sum[s] += v[s];
            t->acc.count[s]++;
        }
    }
}

void hist_add(history_t *h, int64_t t_s, const float values[HIST_SERIES_COUNT], uint8_t valid_mask)
{
    if (t_s < 0) {
        return;
    }
    tier_add(&h->fine, t_s, values, valid_mask);
    tier_add(&h->coarse, t_s, values, valid_mask);
}

uint32_t hist_range_seconds(hist_range_t range)
{
    switch (range) {
    case HIST_RANGE_1H:
        return 3600;
    case HIST_RANGE_6H:
        return 6 * 3600;
    case HIST_RANGE_24H:
        return 24 * 3600;
    default:
        return 7 * 24 * 3600;
    }
}

/* Finds a stored point for bucket `b`, or the in-progress accumulator. */
static bool tier_lookup(const hist_tier_t *t, int64_t b, hist_series_t s, float *v)
{
    if (b == t->cur_bucket) {
        if (t->acc.count[s] == 0) {
            return false;
        }
        *v = t->acc.sum[s] / (float)t->acc.count[s];
        return true;
    }
    if (t->used == 0 || t->cur_bucket < 0) {
        return false;
    }
    /* Points are written in increasing bucket order; the newest stored
     * point sits just before head. Index by distance from it. */
    const uint16_t newest = (uint16_t)((t->head + t->len - 1) % t->len);
    const int64_t newest_b = t->buf[newest].bucket;
    if (newest_b < 0 || b > newest_b) {
        return false;
    }
    /* Fast path: without gaps the bucket sits exactly (newest_b - b) back. */
    const int64_t dist = newest_b - b;
    if (dist < (int64_t)t->used) {
        const hist_point_t *p = &t->buf[(newest + t->len - (uint16_t)dist) % t->len];
        if (p->bucket == b) {
            if (!(p->valid_mask & (1u << s))) {
                return false;
            }
            *v = p->mean[s];
            return true;
        }
    }
    for (uint16_t k = 0; k < t->used; k++) {
        const hist_point_t *p = &t->buf[(newest + t->len - k) % t->len];
        if (p->bucket == b) {
            if (!(p->valid_mask & (1u << s))) {
                return false;
            }
            *v = p->mean[s];
            return true;
        }
        if (p->bucket < b) {
            return false;
        }
    }
    return false;
}

size_t hist_query(const history_t *h, hist_range_t range, int64_t now_s, hist_series_t series, float *out,
                  bool *out_valid, size_t max_points)
{
    if (max_points == 0 || series >= HIST_SERIES_COUNT || now_s < 0) {
        return 0;
    }
    const hist_tier_t *t = (range == HIST_RANGE_7D) ? &h->coarse : &h->fine;
    const int64_t nb = (int64_t)(hist_range_seconds(range) / t->step_s);
    const int64_t last_b = now_s / (int64_t)t->step_s;
    const int64_t first_b = last_b - nb + 1;
    const int64_t per_point = (nb + (int64_t)max_points - 1) / (int64_t)max_points;
    size_t n = 0;
    for (int64_t start = first_b; start <= last_b && n < max_points; start += per_point) {
        float sum = 0;
        int cnt = 0;
        for (int64_t b = start; b < start + per_point && b <= last_b; b++) {
            float v;
            if (b >= 0 && tier_lookup(t, b, series, &v)) {
                sum += v;
                cnt++;
            }
        }
        out_valid[n] = cnt > 0;
        out[n] = cnt > 0 ? sum / (float)cnt : 0.0f;
        n++;
    }
    return n;
}
