#include <string.h>

#include "outage_dial.h"
#include "test_harness.h"

static void test_midnight_and_large_arc(void)
{
    outage_arc_t arc[2];
    CHECK(outage_split_arc((outage_interval_t){1380, 120}, arc) == 2);
    CHECK(arc[0].start_deg == 615 && arc[0].end_deg == 630);
    CHECK(arc[1].start_deg == 270 && arc[1].end_deg == 300);
    CHECK(outage_split_arc((outage_interval_t){60, 900}, arc) == 1);
    CHECK(arc[0].large_arc);
    CHECK(outage_split_arc((outage_interval_t){60, 60}, arc) == 0);
}

static void test_countdown_and_coverage(void)
{
    char text[32];
    outage_format_duration(text, sizeof(text), 140, true);
    CHECK(strcmp(text, "in 2 h 20 m") == 0);
    outage_format_duration(text, sizeof(text), 35, true);
    CHECK(strcmp(text, "in 35 m") == 0);
    CHECK(outage_runtime_minutes(86, 20, 15000, 908) == 654);
    CHECK(outage_shortfall_minutes(654, 140, 15) == 0);
    CHECK(outage_shortfall_minutes(55, 120, 15) == 80);
    CHECK(outage_runtime_minutes(20, 20, 15000, 908) == 0);
    CHECK(outage_runtime_minutes(86, 20, 0, 908) == -1);
    CHECK(outage_runtime_minutes(86, 20, 15000, -1) == -1);
    CHECK(outage_runtime_minutes(86, 20, 15000, 0) == 11880);
}

static void test_schedule_event_and_freshness(void)
{
    outage_mock_set_today(2026, 11, 30, 1796000000);
    outage_set_adapter(outage_mock_get_day);
    outage_day_t today, tomorrow;
    CHECK(outage_get_day(2026, 11, 30, 1796000000, &today));
    CHECK(strcmp(today.group_label, "4.1") == 0);
    CHECK(outage_get_day(2026, 12, 1, 1796000000, &tomorrow));
    bool off;
    int event, duration, next_start, next_end;
    CHECK(outage_find_event(&today, &tomorrow, 19 * 60 + 10, &off, &event, &duration, &next_start, &next_end));
    CHECK(off && event == 21 * 60 + 30 && duration == 140);
    CHECK(next_start == 1440 && next_end == 1650);
    CHECK(!outage_get_day(2026, 11, 30, 1796000000 + 86401, &today));
    outage_set_adapter(NULL);
    CHECK(!outage_get_day(2026, 11, 30, 1796000000, &today));
}

static void test_status_priority(void)
{
    CHECK(outage_status(false, true, true, 10, 40, 20, false) == OUTAGE_STATUS_NO_DATA);
    CHECK(outage_status(true, true, true, 10, 40, 20, false) == OUTAGE_STATUS_FAULT);
    CHECK(outage_status(true, false, true, 19, 40, 20, true) == OUTAGE_STATUS_CRITICAL);
    CHECK(outage_status(true, false, true, 24, 40, 20, true) == OUTAGE_STATUS_LOW);
    CHECK(outage_status(true, false, true, 62, 40, 20, true) == OUTAGE_STATUS_GRID);
    CHECK(outage_status(true, false, true, 86, 40, 20, false) == OUTAGE_STATUS_BATTERY);
}

void suite_outage_dial(void)
{
    RUN(test_midnight_and_large_arc);
    RUN(test_countdown_and_coverage);
    RUN(test_schedule_event_and_freshness);
    RUN(test_status_priority);
}
