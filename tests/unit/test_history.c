#include "history.h"
#include "test_harness.h"

static history_t g_h; /* large: keep off the stack */

static void add_const(int64_t t, float v)
{
    float vals[HIST_SERIES_COUNT] = {v, v, v, v, v};
    hist_add(&g_h, t, vals, 0x1F);
}

static void test_empty(void)
{
    hist_init(&g_h);
    float out[60];
    bool valid[60];
    size_t n = hist_query(&g_h, HIST_RANGE_1H, 3600, HIST_PV, out, valid, 60);
    CHECK(n == 60);
    for (size_t i = 0; i < n; i++) {
        CHECK(!valid[i]);
    }
}

static void test_minute_means(void)
{
    hist_init(&g_h);
    /* minute 0: 100, 200 -> mean 150; minute 1: 300 */
    add_const(0, 100);
    add_const(30, 200);
    add_const(60, 300);
    float out[60];
    bool valid[60];
    size_t n = hist_query(&g_h, HIST_RANGE_1H, 3599, HIST_LOAD, out, valid, 60);
    CHECK(n == 60);
    CHECK(valid[0] && valid[1]);
    CHECK_NEAR(out[0], 150.0, 1e-3);
    CHECK_NEAR(out[1], 300.0, 1e-3); /* current bucket read from accumulator */
    CHECK(!valid[2]);
}

static void test_downsampling(void)
{
    hist_init(&g_h);
    for (int64_t t = 0; t < 6 * 3600; t += 10) {
        add_const(t, (float)(t / 3600)); /* hour index */
    }
    float out[6];
    bool valid[6];
    size_t n = hist_query(&g_h, HIST_RANGE_6H, 6 * 3600 - 1, HIST_SOC, out, valid, 6);
    CHECK(n == 6);
    for (int i = 0; i < 6; i++) {
        CHECK(valid[i]);
        CHECK_NEAR(out[i], (double)i, 1e-3);
    }
}

static void test_ring_wraps_and_bounds(void)
{
    hist_init(&g_h);
    /* 3 days of 1-minute samples: fine tier keeps only the last 24 h */
    for (int64_t t = 0; t < 3 * 86400; t += 60) {
        add_const(t, t < 2 * 86400 ? 1.0f : 2.0f);
    }
    float out[24];
    bool valid[24];
    size_t n = hist_query(&g_h, HIST_RANGE_24H, 3 * 86400 - 1, HIST_GRID, out, valid, 24);
    CHECK(n == 24);
    for (int i = 0; i < 24; i++) {
        CHECK(valid[i]);
        CHECK_NEAR(out[i], 2.0, 1e-3);
    }
    /* 7-day range comes from the coarse tier and sees older data */
    float w[7];
    bool wv[7];
    n = hist_query(&g_h, HIST_RANGE_7D, 3 * 86400 - 1, HIST_GRID, w, wv, 7);
    CHECK(n == 7);
    CHECK(!wv[0] && !wv[3]);
    CHECK(wv[4] && wv[6]);
    CHECK_NEAR(w[4], 1.0, 1e-3);
    CHECK_NEAR(w[6], 2.0, 1e-3);
}

static void test_partial_mask_and_time_reversal(void)
{
    hist_init(&g_h);
    float vals[HIST_SERIES_COUNT] = {10, 20, 30, 40, 50};
    hist_add(&g_h, 120, vals, 1u << HIST_SOC);
    float back[HIST_SERIES_COUNT] = {99, 99, 99, 99, 99};
    hist_add(&g_h, 0, back, 0x1F); /* older bucket: ignored */
    float out[60];
    bool valid[60];
    hist_query(&g_h, HIST_RANGE_1H, 3599, HIST_SOC, out, valid, 60);
    CHECK(!valid[0]);
    CHECK(valid[2]);
    CHECK_NEAR(out[2], 50.0, 1e-3);
    hist_query(&g_h, HIST_RANGE_1H, 3599, HIST_PV, out, valid, 60);
    CHECK(!valid[2]);
}

void suite_history(void)
{
    RUN(test_empty);
    RUN(test_minute_means);
    RUN(test_downsampling);
    RUN(test_ring_wraps_and_bounds);
    RUN(test_partial_mask_and_time_reversal);
}
