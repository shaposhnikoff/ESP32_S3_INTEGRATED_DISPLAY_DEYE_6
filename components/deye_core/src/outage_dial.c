#include "outage_dial.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static outage_get_schedule_fn s_adapter;
static int s_mock_year;
static int s_mock_month;
static int s_mock_day;
static int s_mock_next_year;
static int s_mock_next_month;
static int s_mock_next_day;
static int64_t s_mock_updated;
static char s_mock_group[8] = "4.1";

void outage_set_adapter(outage_get_schedule_fn adapter)
{
    s_adapter = adapter;
}

bool outage_get_day(int year, int month, int day, int64_t now_epoch, outage_day_t *out)
{
    int64_t updated = 0;
    memset(out, 0, sizeof(*out));
    if (s_adapter == NULL || !s_adapter(year, month, day, out, &updated) || out->count > OUTAGE_MAX_INTERVALS ||
        updated <= 0 || updated > now_epoch + 300 || now_epoch - updated > 24 * 60 * 60) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    for (uint8_t i = 0; i < out->count; i++) {
        if (out->intervals[i].start_min >= 1440 || out->intervals[i].end_min > 1440 ||
            out->intervals[i].start_min == out->intervals[i].end_min) {
            memset(out, 0, sizeof(*out));
            return false;
        }
    }
    return true;
}

void outage_mock_set_today(int year, int month, int day, int64_t now_epoch)
{
    s_mock_year = year;
    s_mock_month = month;
    s_mock_day = day;
    s_mock_updated = now_epoch;
    struct tm next = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day + 1, .tm_hour = 12, .tm_isdst = -1};
    mktime(&next);
    s_mock_next_year = next.tm_year + 1900;
    s_mock_next_month = next.tm_mon + 1;
    s_mock_next_day = next.tm_mday;
}

void outage_mock_set_group(const char *group)
{
    if (group == NULL || strlen(group) >= sizeof(s_mock_group))
        return;
    memset(s_mock_group, 0, sizeof(s_mock_group));
    memcpy(s_mock_group, group, strlen(group) + 1);
}

bool outage_mock_get_day(int year, int month, int day, outage_day_t *out, int64_t *updated_epoch)
{
    bool today_date = year == s_mock_year && month == s_mock_month && day == s_mock_day;
    bool tomorrow_date = year == s_mock_next_year && month == s_mock_next_month && day == s_mock_next_day;
    if (!today_date && !tomorrow_date) {
        return false;
    }
    static const outage_interval_t today[] = {{120, 330}, {600, 810}, {1080, 1290}};
    static const outage_interval_t tomorrow[] = {{0, 210}, {480, 690}, {960, 1170}};
    const outage_interval_t *fixture = today_date ? today : tomorrow;
    memset(out, 0, sizeof(*out));
    memcpy(out->intervals, fixture, sizeof(today));
    out->count = 3;
    memcpy(out->group_label, s_mock_group, sizeof(out->group_label));
    *updated_epoch = s_mock_updated;
    return true;
}

static outage_arc_t arc_part(int start, int end)
{
    return (outage_arc_t){
        .start_deg = (int16_t)(270 + start * 360 / 1440),
        .end_deg = (int16_t)(270 + end * 360 / 1440),
        .large_arc = end - start >= 720,
    };
}

size_t outage_split_arc(outage_interval_t interval, outage_arc_t out[2])
{
    if (interval.start_min >= 1440 || interval.end_min > 1440 || interval.start_min == interval.end_min) {
        return 0;
    }
    if (interval.end_min > interval.start_min) {
        out[0] = arc_part(interval.start_min, interval.end_min);
        return 1;
    }
    out[0] = arc_part(interval.start_min, 1440);
    out[1] = arc_part(0, interval.end_min);
    return 2;
}

bool outage_find_event(const outage_day_t *today, const outage_day_t *tomorrow, int now_min, bool *grid_off_now,
                       int *event_min, int *duration_min, int *next_start_min, int *next_end_min)
{
    if (now_min < 0 || now_min >= 1440) {
        return false;
    }
    *grid_off_now = false;
    *event_min = -1;
    *next_start_min = -1;
    *next_end_min = -1;
    for (int d = 0; d < 2; d++) {
        const outage_day_t *day = d == 0 ? today : tomorrow;
        for (uint8_t i = 0; i < day->count; i++) {
            int start = d * 1440 + day->intervals[i].start_min;
            int end = d * 1440 + day->intervals[i].end_min;
            if (end <= start)
                end += 1440;
            if (d == 0 && now_min >= start && now_min < end) {
                *grid_off_now = true;
                *event_min = end;
                *duration_min = end - now_min;
            }
            if (start > now_min && (*next_start_min < 0 || start < *next_start_min)) {
                *next_start_min = start;
                *next_end_min = end;
            }
        }
    }
    if (!*grid_off_now && *next_start_min >= 0) {
        *event_min = *next_start_min;
        *duration_min = *next_end_min - *next_start_min;
    }
    return *event_min >= 0;
}

int outage_runtime_minutes(float soc_pct, float reserve_pct, float capacity_wh, float load_avg_w)
{
    if (!isfinite(soc_pct) || !isfinite(reserve_pct) || !isfinite(capacity_wh) || !isfinite(load_avg_w) ||
        soc_pct < 0 || soc_pct > 100 || reserve_pct < 0 || reserve_pct >= 100 || capacity_wh <= 0 || load_avg_w < 0) {
        return -1;
    }
    const float energy = fmaxf(soc_pct - reserve_pct, 0) * capacity_wh / 100.0f;
    return (int)floorf(energy * 60.0f / fmaxf(load_avg_w, 50.0f));
}

int outage_shortfall_minutes(int runtime_min, int outage_min, int margin_min)
{
    if (runtime_min < 0 || outage_min < 0 || margin_min < 0)
        return -1;
    return outage_min + margin_min > runtime_min ? outage_min + margin_min - runtime_min : 0;
}

void outage_format_duration(char *out, size_t size, int minutes, bool with_prefix)
{
    if (minutes < 0) {
        snprintf(out, size, "--");
    } else if (minutes >= 60) {
        snprintf(out, size, "%s%d h %02d m", with_prefix ? "in " : "", minutes / 60, minutes % 60);
    } else {
        snprintf(out, size, "%s%d m", with_prefix ? "in " : "", minutes);
    }
}

outage_status_t outage_status(bool data_live, bool fault, bool soc_valid, float soc_pct, float warning_pct,
                              float critical_pct, bool grid_on)
{
    if (!data_live)
        return OUTAGE_STATUS_NO_DATA;
    if (fault)
        return OUTAGE_STATUS_FAULT;
    if (soc_valid && soc_pct < critical_pct)
        return OUTAGE_STATUS_CRITICAL;
    if (soc_valid && soc_pct < warning_pct)
        return OUTAGE_STATUS_LOW;
    return grid_on ? OUTAGE_STATUS_GRID : OUTAGE_STATUS_BATTERY;
}
