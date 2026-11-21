#include <string.h>

#include "inverter_model.h"
#include "test_harness.h"

static void parse_fixture(const char *name, inverter_model_t *m, bool *ok)
{
    size_t len;
    char *t = load_fixture(name, &len);
    *ok = im_parse_metrics(t, len, m);
    free(t);
}

static void test_live_capture(void)
{
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_live_2026-11-20.txt", &m, &ok);
    CHECK(ok);
    CHECK(strcmp(m.meta.serial, "TEST000001") == 0);
    CHECK(m.battery.soc_pct.valid);
    CHECK_NEAR(m.battery.soc_pct.value, 94.0, 1e-4);
    CHECK_NEAR(m.battery.voltage_v.value, 53.05, 1e-3);
    CHECK_NEAR(m.battery.power_w.value, 738.0, 1e-3);
    CHECK_NEAR(m.load.power_w.value, 729.0, 1e-3);
    CHECK_NEAR(m.grid.connected.value, 0.0, 1e-6);
    CHECK(m.pv.total_power_w.valid);
    CHECK_NEAR(m.pv.total_power_w.value, 2.0, 1e-3);
    CHECK_NEAR(m.pv.strings[0].voltage_v.value, 134.8, 1e-3);
    CHECK(m.inverter.run_status == IM_RUN_NORMAL);
    CHECK_NEAR(m.inverter.rated_power_w.value, 6000.0, 1e-3);
    CHECK_NEAR(m.inverter.mppts.value, 2.0, 1e-6);
    CHECK(m.tou.enabled.valid && m.tou.enabled.value > 0.5f);
    CHECK(m.tou.day_mask == 0x7F);
    CHECK(m.tou.day_known_mask == 0x7F);
    for (int i = 0; i < IM_TOU_SLOTS; i++) {
        CHECK(m.tou.slots[i].present);
        CHECK_NEAR(m.tou.slots[i].soc_pct.value, 99.0, 1e-4);
        CHECK(m.tou.slots[i].grid_charge.valid);
    }
    CHECK(m.tou.slots[0].start_minutes == 60);
    CHECK(m.tou.slots[5].start_minutes == 1380);
    CHECK(m.meta.scrape_success.valid && m.meta.scrape_success.value > 0.5f);
    CHECK(m.meta.data_age_s.valid);
    /* generator is not exported by the backend */
    CHECK(!m.generator.reported);
    CHECK(!m.generator.power_w.valid);
    CHECK(m.meta.rejected_samples == 0);
    CHECK(m.meta.malformed_lines == 0);
    CHECK(m.meta.known_samples > 100);
}

static void test_energy_balance_sign(void)
{
    /* Documents the battery sign convention used by the UI: with PV ~0 and
     * grid 0, load is supplied by the battery, and battery power is positive. */
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_live_2026-11-20.txt", &m, &ok);
    const float balance = m.pv.total_power_w.value + m.grid.power_w.value + m.battery.power_w.value;
    CHECK(fabs(balance - m.load.power_w.value) / m.load.power_w.value < 0.05);
}

static void test_alarm(void)
{
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_alarm.txt", &m, &ok);
    CHECK(ok);
    CHECK(m.inverter.run_status == IM_RUN_ALARM);
}

static void test_soc_change(void)
{
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_soc75.txt", &m, &ok);
    CHECK(ok);
    CHECK_NEAR(m.battery.soc_pct.value, 75.0, 1e-4);
}

static void test_missing_core(void)
{
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_missing_core.txt", &m, &ok);
    CHECK(!ok);
    CHECK(!m.battery.soc_pct.valid);
    CHECK(m.battery.voltage_v.valid);
}

static void test_invalid_values(void)
{
    inverter_model_t m;
    bool ok;
    parse_fixture("metrics_invalid_values.txt", &m, &ok);
    CHECK(!ok); /* SOC 140 % rejected => no valid core telemetry */
    CHECK(!m.battery.soc_pct.valid);
    CHECK(!m.battery.voltage_v.valid);
    CHECK(!m.grid.power_w.valid);
    CHECK(m.load.power_w.valid);
    CHECK(!m.pv.strings[1].power_w.valid);
    CHECK_NEAR(m.pv.total_power_w.value, 1200.0, 1e-3);
    CHECK(m.inverter.run_status == IM_RUN_UNKNOWN);
    CHECK(m.meta.malformed_lines == 2);
    CHECK(m.meta.rejected_samples == 6);
}

static void test_empty_and_null(void)
{
    inverter_model_t m;
    CHECK(!im_parse_metrics("", 0, &m));
    CHECK(!im_parse_metrics(NULL, 0, &m));
    CHECK(!m.pv.total_power_w.valid);
}

static void test_battery_eta(void)
{
    bool charging = false;
    CHECK(im_battery_eta_minutes(im_val(86), im_val(929), 15000, 20, &charging) == 639);
    CHECK(!charging);
    CHECK(im_battery_eta_minutes(im_val(62), im_val(-1200), 15000, 20, &charging) == 285);
    CHECK(charging);
    CHECK(im_battery_eta_minutes(im_val(20), im_val(929), 15000, 20, NULL) == 0);
    CHECK(im_battery_eta_minutes(im_unknown(), im_val(929), 15000, 20, NULL) == -1);
    CHECK(im_battery_eta_minutes(im_val(86), im_val(0), 15000, 20, NULL) == -1);
    CHECK(im_battery_eta_minutes(im_val(86), im_val(929), 0, 20, NULL) == -1);
}

void suite_inverter_model(void)
{
    RUN(test_live_capture);
    RUN(test_energy_balance_sign);
    RUN(test_alarm);
    RUN(test_soc_change);
    RUN(test_missing_core);
    RUN(test_invalid_values);
    RUN(test_empty_and_null);
    RUN(test_battery_eta);
}
