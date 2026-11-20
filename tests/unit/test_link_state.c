#include "link_state.h"
#include "test_harness.h"

static const link_config_t k_cfg = {
    .stale_after_ms = 30000,
    .offline_after_ms = 60000,
    .backoff_min_ms = 2000,
    .backoff_max_ms = 60000,
};

static void test_never_then_fresh(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    CHECK(link_freshness(&ls, 1000) == LINK_NEVER);
    CHECK(link_data_age_ms(&ls, 1000) < 0);
    CHECK(!link_allows_control(&ls, 1000));
    link_on_success(&ls, 1000, 120, true, true, 2.0f);
    CHECK(link_freshness(&ls, 1000) == LINK_FRESH);
    CHECK(link_data_age_ms(&ls, 4000) == 5000);
    CHECK(link_allows_control(&ls, 4000));
}

static void test_stale_by_age(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    link_on_success(&ls, 0, 10, true, true, 25.0f);
    CHECK(link_freshness(&ls, 4000) == LINK_FRESH);
    CHECK(link_freshness(&ls, 6000) == LINK_STALE);
    CHECK(!link_allows_control(&ls, 6000));
}

static void test_backend_poll_failed_is_stale(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    link_on_success(&ls, 0, 10, false, true, 600.0f);
    CHECK(link_freshness(&ls, 0) == LINK_STALE);
    CHECK(!link_allows_control(&ls, 0));
}

static void test_unknown_backend_age_is_not_live(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    link_on_success(&ls, 1000, 10, true, false, 0);
    CHECK(link_freshness(&ls, 1000) == LINK_STALE);
    CHECK(link_data_age_ms(&ls, 1000) == -1);
    CHECK(!link_allows_control(&ls, 1000));
}

static void test_offline_after_failures(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    link_on_success(&ls, 0, 10, true, true, 0.0f);
    link_on_failure(&ls, 5000, -1);
    CHECK(link_freshness(&ls, 5000) == LINK_FRESH); /* single miss is tolerated */
    CHECK(link_freshness(&ls, 61000) == LINK_OFFLINE);
    CHECK(ls.consecutive_failures == 1);
    link_on_success(&ls, 62000, 10, true, true, 0.0f);
    CHECK(ls.consecutive_failures == 0);
    CHECK(ls.total_failures == 1);
}

static void test_backoff(void)
{
    link_state_t ls;
    link_init(&ls, &k_cfg);
    CHECK(link_next_delay_ms(&ls, 3000, 0) == 3000);
    link_on_failure(&ls, 0, -1);
    CHECK(link_next_delay_ms(&ls, 3000, 0) == 2000);
    link_on_failure(&ls, 0, -1);
    CHECK(link_next_delay_ms(&ls, 3000, 0) == 4000);
    for (int i = 0; i < 20; i++) {
        link_on_failure(&ls, 0, -1);
    }
    CHECK(link_next_delay_ms(&ls, 3000, 0) == 60000);
    CHECK(link_next_delay_ms(&ls, 3000, 0.5f) == 66000);
}

void suite_link_state(void)
{
    RUN(test_never_then_fresh);
    RUN(test_stale_by_age);
    RUN(test_backend_poll_failed_is_stale);
    RUN(test_unknown_backend_age_is_not_live);
    RUN(test_offline_after_failures);
    RUN(test_backoff);
}
