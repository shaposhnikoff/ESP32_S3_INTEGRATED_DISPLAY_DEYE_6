#include <stdio.h>
#include <string.h>
#include <time.h>

#include "outage_dial.h"
#include "test_harness.h"

static void test_real_mirror_shape(void)
{
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    time_t midnight = mktime(&local);
    struct tm utc;
    gmtime_r(&now, &utc);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    char body[2048];
    int used = snprintf(body, sizeof(body),
                        "{\"lastUpdated\":\"%s\",\"lastUpdateStatus\":{\"ok\":true},"
                        "\"fact\":{\"data\":{\"%lld\":{\"GPV4.1\":{",
                        timestamp, (long long)midnight);
    for (int hour = 1; hour <= 24; hour++) {
        const char *state = hour >= 7 && hour <= 9     ? "no"
                            : hour == 16               ? "second"
                            : hour >= 17 && hour <= 19 ? "no"
                            : hour == 20               ? "first"
                                                       : "yes";
        used +=
            snprintf(body + used, sizeof(body) - (size_t)used, "\"%d\":\"%s\"%s", hour, state, hour == 24 ? "" : ",");
    }
    used += snprintf(body + used, sizeof(body) - (size_t)used, "}}}}}");
    CHECK(used > 0 && (size_t)used < sizeof(body));
    outage_schedule_t schedule;
    CHECK(outage_parse_mirror(body, (size_t)used, "GPV4.1", now, &schedule));
    CHECK(schedule.days[0].present);
    CHECK(!schedule.days[1].present);
    CHECK(strcmp(schedule.days[0].schedule.group_label, "4.1") == 0);
    CHECK(schedule.days[0].schedule.count == 2);
    CHECK(schedule.days[0].schedule.intervals[0].start_min == 360);
    CHECK(schedule.days[0].schedule.intervals[0].end_min == 540);
    CHECK(schedule.days[0].schedule.intervals[1].start_min == 930);
    CHECK(schedule.days[0].schedule.intervals[1].end_min == 1170);
    CHECK(!outage_parse_mirror(body, (size_t)used, "GPV3.1", now, &schedule));
    CHECK(!outage_parse_mirror(body, (size_t)used, "GPV4.1", now + 86401, &schedule));
}

void suite_outage_json(void)
{
    RUN(test_real_mirror_shape);
}
