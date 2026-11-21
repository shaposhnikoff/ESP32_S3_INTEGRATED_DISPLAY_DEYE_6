#include "inverter_model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "prom_parser.h"

typedef struct {
    const char *name;
    size_t offset; /* offset of im_value_t inside inverter_model_t */
    float min;
    float max;
} metric_map_t;

#define F(field) offsetof(inverter_model_t, field)

/* Plausibility ranges reject corrupted reads, not operating limits. */
static const metric_map_t k_metrics[] = {
    {"deye_battery_percent", F(battery.soc_pct), 0, 100},
    {"deye_battery_voltage_volts", F(battery.voltage_v), 0, 100},
    {"deye_battery_current_amperes", F(battery.current_a), -400, 400},
    {"deye_battery_power_watts", F(battery.power_w), -20000, 20000},
    {"deye_battery_temperature_celsius", F(battery.temperature_c), -40, 100},
    {"deye_daily_battery_charge_kwh", F(battery.daily_charge_kwh), 0, 1000},
    {"deye_daily_battery_discharge_kwh", F(battery.daily_discharge_kwh), 0, 1000},
    {"deye_total_battery_charge_kwh", F(battery.total_charge_kwh), 0, 1e7f},
    {"deye_total_battery_discharge_kwh", F(battery.total_discharge_kwh), 0, 1e7f},

    {"deye_pv1_voltage_volts", F(pv.strings[0].voltage_v), 0, 1000},
    {"deye_pv1_current_amperes", F(pv.strings[0].current_a), 0, 100},
    {"deye_pv1_power_watts", F(pv.strings[0].power_w), 0, 20000},
    {"deye_pv2_voltage_volts", F(pv.strings[1].voltage_v), 0, 1000},
    {"deye_pv2_current_amperes", F(pv.strings[1].current_a), 0, 100},
    {"deye_pv2_power_watts", F(pv.strings[1].power_w), 0, 20000},
    {"deye_pv3_voltage_volts", F(pv.strings[2].voltage_v), 0, 1000},
    {"deye_pv3_current_amperes", F(pv.strings[2].current_a), 0, 100},
    {"deye_pv3_power_watts", F(pv.strings[2].power_w), 0, 20000},
    {"deye_pv4_voltage_volts", F(pv.strings[3].voltage_v), 0, 1000},
    {"deye_pv4_current_amperes", F(pv.strings[3].current_a), 0, 100},
    {"deye_pv4_power_watts", F(pv.strings[3].power_w), 0, 20000},
    {"deye_daily_pv_production_kwh", F(pv.daily_kwh), 0, 1000},
    {"deye_total_pv_production_kwh", F(pv.total_kwh), 0, 1e7f},

    {"deye_grid_voltage_volts", F(grid.voltage_v), 0, 400},
    {"deye_grid_frequency_hertz", F(grid.frequency_hz), 0, 70},
    {"deye_grid_power_watts", F(grid.power_w), -30000, 30000},
    {"deye_external_ct_power_watts", F(grid.ct_power_w), -30000, 30000},
    {"deye_grid_connected_status", F(grid.connected), 0, 1},
    {"deye_daily_energy_bought_kwh", F(grid.daily_bought_kwh), 0, 1000},
    {"deye_daily_energy_sold_kwh", F(grid.daily_sold_kwh), 0, 1000},
    {"deye_total_energy_sold_kwh", F(grid.total_sold_kwh), 0, 1e7f},

    {"deye_load_power_watts", F(load.power_w), 0, 30000},
    {"deye_load_l1_voltage_volts", F(load.voltage_v), 0, 400},
    {"deye_load_l1_current_amperes", F(load.current_a), 0, 200},
    {"deye_load_frequency_hertz", F(load.frequency_hz), 0, 70},
    {"deye_daily_load_consumption_kwh", F(load.daily_kwh), 0, 1000},
    {"deye_total_consumption_kwh", F(load.total_kwh), 0, 1e7f},

    {"deye_output_power_watts", F(inverter.output_power_w), -30000, 30000},
    {"deye_output_l1_voltage_volts", F(inverter.output_voltage_v), 0, 400},
    {"deye_output_l1_current_amperes", F(inverter.output_current_a), 0, 200},
    {"deye_output_frequency_hertz", F(inverter.output_frequency_hz), 0, 70},
    {"deye_dc_temperature_celsius", F(inverter.dc_temperature_c), -40, 150},
    {"deye_ac_temperature_celsius", F(inverter.ac_temperature_c), -40, 150},
    {"deye_device_rated_power_watts", F(inverter.rated_power_w), 0, 100000},
    {"deye_device_mppts", F(inverter.mppts), 0, 8},
    {"deye_device_phases", F(inverter.phases), 0, 3},

    {"deye_tou_enabled", F(tou.enabled), 0, 1},

    {"deye_scrape_success", F(meta.scrape_success), 0, 1},
    {"deye_data_age_seconds", F(meta.data_age_s), 0, 1e9f},
    {"deye_last_successful_scrape_timestamp_seconds", F(meta.last_success_unix_s), 0, 1e10f},
};

#undef F

static const char *const k_days[7] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

static bool set_checked(im_value_t *dst, double v, float min, float max)
{
    if (isnan(v) || isinf(v) || v < min || v > max) {
        dst->valid = false;
        return false;
    }
    dst->value = (float)v;
    dst->valid = true;
    return true;
}

static int slot_index(const prom_sample_t *s)
{
    char buf[8];
    if (!prom_label_get(s, "slot", buf, sizeof(buf))) {
        return -1;
    }
    char *end = NULL;
    long n = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || n < 1 || n > IM_TOU_SLOTS) {
        return -1;
    }
    return (int)(n - 1);
}

static void capture_serial(inverter_model_t *m, const prom_sample_t *s)
{
    if (m->meta.serial[0] == '\0') {
        prom_label_get(s, "serial", m->meta.serial, sizeof(m->meta.serial));
    }
}

/* Returns 1 = accepted, 0 = not a known metric, -1 = known but rejected. */
static int handle_tou(inverter_model_t *m, const prom_sample_t *s)
{
    if (prom_name_is(s, "deye_tou_day")) {
        char day[8];
        if (!prom_label_get(s, "day", day, sizeof(day))) {
            return -1;
        }
        for (int i = 0; i < 7; i++) {
            if (strcmp(day, k_days[i]) == 0) {
                m->tou.day_known_mask |= (uint8_t)(1u << i);
                if (s->value >= 0.5) {
                    m->tou.day_mask |= (uint8_t)(1u << i);
                }
                return 1;
            }
        }
        return -1;
    }

    static const char *const slot_metrics[] = {
        "deye_tou_slot_start_minutes",    "deye_tou_slot_power_watts", "deye_tou_slot_voltage_volts",
        "deye_tou_slot_capacity_percent", "deye_tou_charge",
    };
    int which = -1;
    for (int i = 0; i < (int)(sizeof(slot_metrics) / sizeof(slot_metrics[0])); i++) {
        if (prom_name_is(s, slot_metrics[i])) {
            which = i;
            break;
        }
    }
    if (which < 0) {
        /* Informational duplicates of the slot data. */
        if (prom_name_is(s, "deye_tou_slot_start_hhmm") || prom_name_is(s, "deye_tou_slot_info") ||
            prom_name_is(s, "deye_tou_mode")) {
            return 1;
        }
        return 0;
    }
    const int idx = slot_index(s);
    if (idx < 0) {
        return -1;
    }
    im_tou_slot_t *slot = &m->tou.slots[idx];
    bool ok = true;
    switch (which) {
    case 0:
        if (s->value >= 0 && s->value < 24 * 60) {
            slot->start_minutes = (uint16_t)s->value;
            slot->present = true;
        } else {
            ok = false;
        }
        break;
    case 1:
        ok = set_checked(&slot->power_w, s->value, 0, 100000);
        break;
    case 2:
        ok = set_checked(&slot->voltage_v, s->value, 0, 100);
        break;
    case 3:
        ok = set_checked(&slot->soc_pct, s->value, 0, 100);
        break;
    case 4: {
        char src[8];
        if (!prom_label_get(s, "source", src, sizeof(src))) {
            return -1;
        }
        if (strcmp(src, "grid") == 0) {
            ok = set_checked(&slot->grid_charge, s->value, 0, 1);
        } else if (strcmp(src, "gen") == 0) {
            ok = set_checked(&slot->gen_charge, s->value, 0, 1);
        } else {
            return -1;
        }
        break;
    }
    default:
        break;
    }
    return ok ? 1 : -1;
}

static int handle_run_status(inverter_model_t *m, const prom_sample_t *s)
{
    static const struct {
        const char *label;
        im_run_status_t status;
    } k_status[] = {
        {"standby", IM_RUN_STANDBY}, {"selfcheck", IM_RUN_SELFCHECK}, {"normal", IM_RUN_NORMAL},
        {"alarm", IM_RUN_ALARM},     {"fault", IM_RUN_FAULT},
    };
    char st[16];
    if (!prom_label_get(s, "status", st, sizeof(st))) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(k_status) / sizeof(k_status[0]); i++) {
        if (strcmp(st, k_status[i].label) == 0) {
            if (s->value >= 0.5) {
                m->inverter.run_status = k_status[i].status;
            }
            return 1;
        }
    }
    return -1;
}

static bool on_sample(const prom_sample_t *s, void *ctx)
{
    inverter_model_t *m = ctx;
    if (s->name_len < 5 || memcmp(s->name, "deye_", 5) != 0) {
        m->meta.ignored_samples++;
        return true;
    }
    capture_serial(m, s);

    for (size_t i = 0; i < sizeof(k_metrics) / sizeof(k_metrics[0]); i++) {
        if (prom_name_is(s, k_metrics[i].name)) {
            im_value_t *dst = (im_value_t *)((char *)m + k_metrics[i].offset);
            if (set_checked(dst, s->value, k_metrics[i].min, k_metrics[i].max)) {
                m->meta.known_samples++;
            } else {
                m->meta.rejected_samples++;
            }
            return true;
        }
    }

    int r = 0;
    if (prom_name_is(s, "deye_running_status")) {
        r = handle_run_status(m, s);
    } else if (s->name_len > 9 && memcmp(s->name, "deye_tou_", 9) == 0) {
        r = handle_tou(m, s);
    }
    if (r > 0) {
        m->meta.known_samples++;
    } else if (r < 0) {
        m->meta.rejected_samples++;
    } else {
        m->meta.ignored_samples++;
    }
    return true;
}

void im_reset(inverter_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->inverter.run_status = IM_RUN_UNKNOWN;
}

bool im_parse_metrics(const char *text, size_t len, inverter_model_t *m)
{
    im_reset(m);
    prom_parse_stats_t st = prom_parse(text, len, on_sample, m);
    m->meta.malformed_lines = st.malformed;

    float sum = 0;
    bool any = false;
    for (int i = 0; i < IM_PV_STRINGS; i++) {
        if (m->pv.strings[i].power_w.valid) {
            sum += m->pv.strings[i].power_w.value;
            any = true;
        }
    }
    m->pv.total_power_w.valid = any;
    m->pv.total_power_w.value = any ? sum : 0.0f;

    return m->battery.soc_pct.valid && m->load.power_w.valid;
}

const char *im_run_status_name(im_run_status_t s)
{
    switch (s) {
    case IM_RUN_STANDBY:
        return "Standby";
    case IM_RUN_SELFCHECK:
        return "Self-check";
    case IM_RUN_NORMAL:
        return "Normal";
    case IM_RUN_ALARM:
        return "Alarm";
    case IM_RUN_FAULT:
        return "Fault";
    default:
        return "Unknown";
    }
}

int32_t im_battery_eta_minutes(im_value_t soc_pct, im_value_t power_w, uint16_t capacity_wh, uint8_t cutoff_pct,
                               bool *charging)
{
    if (!soc_pct.valid || !power_w.valid || capacity_wh == 0 || cutoff_pct >= 100 || !isfinite(soc_pct.value) ||
        !isfinite(power_w.value) || soc_pct.value < 0 || soc_pct.value > 100 || fabsf(power_w.value) <= 20.0f) {
        return -1;
    }
    const bool is_charging = power_w.value < 0;
    if (charging != NULL) {
        *charging = is_charging;
    }
    const float remaining_pct = is_charging ? 100.0f - soc_pct.value : soc_pct.value - cutoff_pct;
    const float hours = fmaxf(remaining_pct, 0) * capacity_wh / (100.0f * fabsf(power_w.value));
    return (int32_t)fminf(hours * 60.0f + 0.5f, 5999.0f);
}
