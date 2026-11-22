#include "outage_dial.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *first;
    const char *last;
} span_t;

static void whitespace(const char **p, const char *end)
{
    while (*p < end && isspace((unsigned char)**p))
        (*p)++;
}

static bool string(const char **p, const char *end, span_t *out)
{
    if (*p >= end || **p != '"')
        return false;
    const char *start = ++*p;
    while (*p < end) {
        if (**p == '"') {
            *out = (span_t){start, *p};
            (*p)++;
            return true;
        }
        if ((unsigned char)**p < 0x20)
            return false;
        if (**p == '\\') {
            (*p)++;
            if (*p >= end)
                return false;
        }
        (*p)++;
    }
    return false;
}

static bool value(const char **p, const char *end, int depth)
{
    if (depth > 32)
        return false;
    whitespace(p, end);
    if (*p >= end)
        return false;
    if (**p == '"') {
        span_t ignored;
        return string(p, end, &ignored);
    }
    if (**p == '{' || **p == '[') {
        const char closing = **p == '{' ? '}' : ']';
        const bool object = **p == '{';
        (*p)++;
        whitespace(p, end);
        if (*p < end && **p == closing) {
            (*p)++;
            return true;
        }
        for (;;) {
            if (object) {
                span_t ignored;
                if (!string(p, end, &ignored))
                    return false;
                whitespace(p, end);
                if (*p >= end || **p != ':')
                    return false;
                (*p)++;
            }
            if (!value(p, end, depth + 1))
                return false;
            whitespace(p, end);
            if (*p >= end)
                return false;
            if (**p == closing) {
                (*p)++;
                return true;
            }
            if (**p != ',')
                return false;
            (*p)++;
            whitespace(p, end);
        }
    }
    const char *start = *p;
    while (*p < end && !isspace((unsigned char)**p) && **p != ',' && **p != '}' && **p != ']')
        (*p)++;
    return *p > start;
}

static bool equal(span_t span, const char *text)
{
    return (size_t)(span.last - span.first) == strlen(text) && memcmp(span.first, text, strlen(text)) == 0;
}

static bool member(span_t object, const char *key, span_t *out)
{
    const char *p = object.first;
    if (p >= object.last || *p++ != '{')
        return false;
    whitespace(&p, object.last);
    while (p < object.last && *p != '}') {
        span_t name;
        if (!string(&p, object.last, &name))
            return false;
        whitespace(&p, object.last);
        if (p >= object.last || *p++ != ':')
            return false;
        whitespace(&p, object.last);
        const char *start = p;
        if (!value(&p, object.last, 0))
            return false;
        if (equal(name, key)) {
            *out = (span_t){start, p};
            return true;
        }
        whitespace(&p, object.last);
        if (p < object.last && *p == ',') {
            p++;
            whitespace(&p, object.last);
        } else
            break;
    }
    return false;
}

static bool text(span_t json_string, char *out, size_t size)
{
    const size_t length = (size_t)(json_string.last - json_string.first);
    if (length < 2 || json_string.first[0] != '"' || json_string.last[-1] != '"' || length - 1 > size)
        return false;
    memcpy(out, json_string.first + 1, length - 2);
    out[length - 2] = 0;
    return true;
}

static int64_t civil_days(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + doe - 719468;
}

static bool updated_at(span_t timestamp, int64_t *epoch)
{
    char iso[40];
    if (!text(timestamp, iso, sizeof(iso)))
        return false;
    int year, month, day, hour, minute, second;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 || year < 2024 ||
        month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60 ||
        strchr(iso, 'Z') == NULL)
        return false;
    *epoch = civil_days(year, (unsigned)month, (unsigned)day) * 86400 + hour * 3600 + minute * 60 + second;
    return true;
}

static bool add_interval(outage_day_t *day, int start, int end)
{
    if (day->count > 0 && day->intervals[day->count - 1].end_min == start) {
        day->intervals[day->count - 1].end_min = (uint16_t)end;
        return true;
    }
    if (day->count >= OUTAGE_MAX_INTERVALS)
        return false;
    day->intervals[day->count++] = (outage_interval_t){(uint16_t)start, (uint16_t)end};
    return true;
}

static bool parse_hours(span_t hours, const char *group, outage_day_t *day)
{
    memset(day, 0, sizeof(*day));
    const char *label = strncmp(group, "GPV", 3) == 0 ? group + 3 : group;
    if (strlen(label) >= sizeof(day->group_label))
        return false;
    memcpy(day->group_label, label, strlen(label) + 1);
    for (int hour = 1; hour <= 24; hour++) {
        char key[4], state[12];
        span_t item;
        snprintf(key, sizeof(key), "%d", hour);
        if (!member(hours, key, &item) || !text(item, state, sizeof(state)))
            return false;
        const int base = (hour - 1) * 60;
        if (strcmp(state, "no") == 0) {
            if (!add_interval(day, base, base + 60))
                return false;
        } else if (strcmp(state, "first") == 0) {
            if (!add_interval(day, base, base + 30))
                return false;
        } else if (strcmp(state, "second") == 0) {
            if (!add_interval(day, base + 30, base + 60))
                return false;
        } else if (strcmp(state, "yes") != 0 && strcmp(state, "maybe") != 0 && strcmp(state, "mfirst") != 0 &&
                   strcmp(state, "msecond") != 0) {
            return false;
        }
    }
    return true;
}

bool outage_parse_mirror(const char *json, size_t length, const char *group, int64_t now_epoch, outage_schedule_t *out)
{
    if (json == NULL || group == NULL || out == NULL || length == 0 || length > 128 * 1024 || now_epoch < 1704067200)
        return false;
    memset(out, 0, sizeof(*out));
    const char *p = json;
    const char *end = json + length;
    if (!value(&p, end, 0))
        return false;
    whitespace(&p, end);
    if (p != end)
        return false;
    const span_t root = {json, end};
    span_t timestamp, status, ok, fact, data;
    if (!member(root, "lastUpdated", &timestamp) || !updated_at(timestamp, &out->updated_epoch) ||
        out->updated_epoch > now_epoch + 300 || now_epoch - out->updated_epoch > 86400 ||
        !member(root, "lastUpdateStatus", &status) || !member(status, "ok", &ok) || !equal(ok, "true") ||
        !member(root, "fact", &fact) || !member(fact, "data", &data))
        return false;

    time_t current = (time_t)now_epoch;
    struct tm today, tomorrow;
    if (localtime_r(&current, &today) == NULL)
        return false;
    tomorrow = today;
    tomorrow.tm_mday++;
    tomorrow.tm_hour = 12;
    tomorrow.tm_isdst = -1;
    mktime(&tomorrow);
    struct tm *dates[] = {&today, &tomorrow};
    for (int i = 0; i < 2; i++) {
        out->days[i].year = dates[i]->tm_year + 1900;
        out->days[i].month = dates[i]->tm_mon + 1;
        out->days[i].day = dates[i]->tm_mday;
    }

    p = data.first;
    if (p >= data.last || *p++ != '{')
        return false;
    whitespace(&p, data.last);
    while (p < data.last && *p != '}') {
        span_t key;
        if (!string(&p, data.last, &key))
            return false;
        whitespace(&p, data.last);
        if (p >= data.last || *p++ != ':')
            return false;
        whitespace(&p, data.last);
        const char *start = p;
        if (!value(&p, data.last, 0))
            return false;
        span_t groups = {start, p};
        char seconds[16];
        if (text((span_t){key.first - 1, key.last + 1}, seconds, sizeof(seconds)) && strlen(seconds) == 10 &&
            strspn(seconds, "0123456789") == 10) {
            time_t day_epoch = (time_t)strtoll(seconds, NULL, 10);
            struct tm day_time;
            if (localtime_r(&day_epoch, &day_time) == NULL || day_time.tm_hour != 0 || day_time.tm_min != 0)
                return false;
            for (int i = 0; i < 2; i++) {
                outage_dated_day_t *target = &out->days[i];
                if (target->year == day_time.tm_year + 1900 && target->month == day_time.tm_mon + 1 &&
                    target->day == day_time.tm_mday) {
                    span_t hours;
                    if (!member(groups, group, &hours) || !parse_hours(hours, group, &target->schedule))
                        return false;
                    target->present = true;
                }
            }
        }
        whitespace(&p, data.last);
        if (p < data.last && *p == ',') {
            p++;
            whitespace(&p, data.last);
        } else
            break;
    }
    return out->days[0].present;
}
