/*
 * Typed inverter model populated from the Deye Prometheus exporter.
 *
 * Every measurement is an im_value_t: a value is only meaningful when
 * `valid` is true. Missing, NaN or out-of-range metrics stay invalid and must
 * be rendered as "unknown", never as zero.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IM_PV_STRINGS 4
#define IM_TOU_SLOTS 6
#define IM_SERIAL_LEN 24

typedef struct {
    float value;
    bool valid;
} im_value_t;

typedef enum {
    IM_RUN_UNKNOWN = 0,
    IM_RUN_STANDBY,
    IM_RUN_SELFCHECK,
    IM_RUN_NORMAL,
    IM_RUN_ALARM,
    IM_RUN_FAULT,
} im_run_status_t;

typedef struct {
    im_value_t soc_pct;
    im_value_t voltage_v;
    im_value_t current_a;
    /* Exporter sign convention: positive = discharging (verified on live
     * data by energy balance, see docs/api-contract.md). */
    im_value_t power_w;
    im_value_t temperature_c;
    im_value_t daily_charge_kwh;
    im_value_t daily_discharge_kwh;
    im_value_t total_charge_kwh;
    im_value_t total_discharge_kwh;
} im_battery_t;

typedef struct {
    im_value_t voltage_v;
    im_value_t current_a;
    im_value_t power_w;
} im_pv_string_t;

typedef struct {
    im_pv_string_t strings[IM_PV_STRINGS];
    im_value_t total_power_w; /* derived: sum of valid string powers */
    im_value_t daily_kwh;
    im_value_t total_kwh;
} im_pv_t;

typedef struct {
    im_value_t voltage_v;
    im_value_t frequency_hz;
    /* Sign convention of grid power is NOT verified; see api-contract.md. */
    im_value_t power_w;
    im_value_t ct_power_w;
    im_value_t connected; /* 1 = connected, 0 = disconnected */
    im_value_t daily_bought_kwh;
    im_value_t daily_sold_kwh;
    im_value_t total_sold_kwh;
} im_grid_t;

typedef struct {
    im_value_t power_w;
    im_value_t voltage_v;
    im_value_t current_a;
    im_value_t frequency_hz;
    im_value_t daily_kwh;
    im_value_t total_kwh;
} im_load_t;

typedef struct {
    im_value_t output_power_w;
    im_value_t output_voltage_v;
    im_value_t output_current_a;
    im_value_t output_frequency_hz;
    im_value_t dc_temperature_c;
    im_value_t ac_temperature_c;
    im_value_t rated_power_w;
    im_value_t mppts;
    im_value_t phases;
    im_run_status_t run_status;
} im_inverter_t;

typedef struct {
    bool present;
    uint16_t start_minutes;
    im_value_t power_w;
    im_value_t voltage_v;
    im_value_t soc_pct;
    im_value_t grid_charge;
    im_value_t gen_charge;
} im_tou_slot_t;

typedef struct {
    im_value_t enabled;
    uint8_t day_mask;       /* bit0 = Monday ... bit6 = Sunday */
    uint8_t day_known_mask; /* which day bits were reported */
    im_tou_slot_t slots[IM_TOU_SLOTS];
} im_tou_t;

/* The exporter currently exposes no generator metrics. All fields stay
 * invalid and `reported` stays false until the backend adds them. */
typedef struct {
    bool reported;
    im_value_t voltage_v;
    im_value_t frequency_hz;
    im_value_t power_w;
} im_generator_t;

typedef struct {
    char serial[IM_SERIAL_LEN];
    im_value_t scrape_success;      /* exporter's own Modbus poll result */
    im_value_t data_age_s;          /* seconds since exporter's last good poll */
    im_value_t last_success_unix_s; /* exporter's wall clock */
    size_t known_samples;
    size_t ignored_samples;
    size_t rejected_samples; /* known metric, invalid value */
    size_t malformed_lines;
} im_meta_t;

typedef struct {
    im_battery_t battery;
    im_pv_t pv;
    im_grid_t grid;
    im_load_t load;
    im_inverter_t inverter;
    im_tou_t tou;
    im_generator_t generator;
    im_meta_t meta;
} inverter_model_t;

void im_reset(inverter_model_t *m);

/*
 * Parses exporter text into `m` (fully reset first). Returns true when the
 * payload contained the mandatory core metrics (battery SOC and load power);
 * false means the payload must not be treated as valid telemetry.
 */
bool im_parse_metrics(const char *text, size_t len, inverter_model_t *m);

const char *im_run_status_name(im_run_status_t s);

/* Estimated minutes to cutoff while discharging or to full while charging.
 * Returns -1 when inputs are missing or battery power is near idle. */
int32_t im_battery_eta_minutes(im_value_t soc_pct, im_value_t power_w, uint16_t capacity_wh, uint8_t cutoff_pct,
                               bool *charging);

static inline im_value_t im_val(float v)
{
    im_value_t r = {v, true};
    return r;
}

static inline im_value_t im_unknown(void)
{
    im_value_t r = {0.0f, false};
    return r;
}

#ifdef __cplusplus
}
#endif
