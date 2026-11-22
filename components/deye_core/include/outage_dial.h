#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OUTAGE_MAX_INTERVALS 8

typedef struct {
    uint16_t start_min;
    uint16_t end_min;
} outage_interval_t;

typedef struct {
    outage_interval_t intervals[OUTAGE_MAX_INTERVALS];
    uint8_t count;
    char group_label[8];
} outage_day_t;

typedef struct {
    int year;
    int month;
    int day;
    bool present;
    outage_day_t schedule;
} outage_dated_day_t;

typedef struct {
    outage_dated_day_t days[2];
    int64_t updated_epoch;
} outage_schedule_t;

typedef struct {
    int16_t start_deg;
    int16_t end_deg;
    bool large_arc;
} outage_arc_t;

typedef bool (*outage_get_schedule_fn)(int year, int month, int day, outage_day_t *out, int64_t *updated_epoch);

typedef enum {
    OUTAGE_STATUS_NO_DATA,
    OUTAGE_STATUS_FAULT,
    OUTAGE_STATUS_LOW,
    OUTAGE_STATUS_CRITICAL,
    OUTAGE_STATUS_BATTERY,
    OUTAGE_STATUS_GRID,
} outage_status_t;

/* The adapter supplies a validated schedule snapshot from the application. */
void outage_set_adapter(outage_get_schedule_fn adapter);
bool outage_get_day(int year, int month, int day, int64_t now_epoch, outage_day_t *out);
bool outage_mock_get_day(int year, int month, int day, outage_day_t *out, int64_t *updated_epoch);
void outage_mock_set_today(int year, int month, int day, int64_t now_epoch);
void outage_mock_set_group(const char *group);
/* Parses confirmed outages for today and tomorrow from the Kyiv-region mirror. */
bool outage_parse_mirror(const char *json, size_t length, const char *group, int64_t now_epoch, outage_schedule_t *out);

size_t outage_split_arc(outage_interval_t interval, outage_arc_t out[2]);
bool outage_find_event(const outage_day_t *today, const outage_day_t *tomorrow, int now_min, bool *grid_off_now,
                       int *event_min, int *duration_min, int *next_start_min, int *next_end_min);
int outage_runtime_minutes(float soc_pct, float reserve_pct, float capacity_wh, float load_avg_w);
int outage_shortfall_minutes(int runtime_min, int outage_min, int margin_min);
void outage_format_duration(char *out, size_t size, int minutes, bool with_prefix);
outage_status_t outage_status(bool data_live, bool fault, bool soc_valid, float soc_pct, float warning_pct,
                              float critical_pct, bool grid_on);
