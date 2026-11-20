# Backend API contract

Status: **observed**, not specified by the backend owner. Everything below was
taken from live responses of `http://192.168.10.39:9101` on 2026-11-20
(captures in `tests/fixtures/`). If the exporter changes, update the fixtures
and `components/deye_core/src/inverter_model.c`.

## Endpoints

| Method | Path | Used by HMI | Notes |
|---|---|---|---|
| GET | `/metrics` | yes, every poll | Prometheus text format 0.0.4, ~300 lines, ~15 KB |
| GET | `/health` | no (documented only) | `{"ok", "serial", "age_seconds", "last_success", "error"}` |
| any | `/`, `/api`, `/docs`, `/openapi.json` | - | 404 |

Ports 80, 3000, 5000, 8000, 8080, 9090 on the Raspberry Pi did not answer
HTTP. **No write API was found.** The "separate API capable of writing
Modbus registers" mentioned in the requirements was not reachable or
documented, so the HMI has no write transport (see control-safety.md).

Transport: plain HTTP, no authentication, no TLS. The HMI performs GET
requests only.

## Schema / versioning

The exporter has no explicit schema version. The HMI treats the set of metric
names as the schema: unknown `deye_*` metrics are counted as *ignored*, known
metrics with impossible values as *rejected* (both visible on the
Diagnostics screen). A payload is accepted as telemetry only if it contains a
valid `deye_battery_percent` and `deye_load_power_watts`.

## Metric mapping

All samples carry `serial="<inverter S/N>"`. Plausibility ranges are in
`k_metrics[]` in `inverter_model.c`.

| Metric | Model field | Unit | Semantics |
|---|---|---|---|
| `deye_battery_percent` | battery.soc_pct | % | SOC |
| `deye_battery_voltage_volts` | battery.voltage_v | V | |
| `deye_battery_current_amperes` | battery.current_a | A | |
| `deye_battery_power_watts` | battery.power_w | W | **+ = discharging** (see below) |
| `deye_battery_temperature_celsius` | battery.temperature_c | C | |
| `deye_{daily,total}_battery_{charge,discharge}_kwh` | battery.* | kWh | |
| `deye_pv{1..4}_{voltage_volts,current_amperes,power_watts}` | pv.strings[0..3] | V/A/W | |
| (derived) | pv.total_power_w | W | sum of valid string powers |
| `deye_{daily,total}_pv_production_kwh` | pv.* | kWh | |
| `deye_grid_voltage_volts`, `deye_grid_frequency_hertz` | grid.* | V/Hz | |
| `deye_grid_power_watts` | grid.power_w | W | **sign unverified** |
| `deye_external_ct_power_watts` | grid.ct_power_w | W | sign unverified |
| `deye_grid_connected_status` | grid.connected | 0/1 | |
| `deye_daily_energy_{bought,sold}_kwh`, `deye_total_energy_sold_kwh` | grid.* | kWh | |
| `deye_load_power_watts`, `deye_load_l1_{voltage,current}`, `deye_load_frequency_hertz` | load.* | | |
| `deye_{daily_load,total}_consumption_kwh` | load.* | kWh | |
| `deye_output_*`, `deye_{dc,ac}_temperature_celsius` | inverter.* | | |
| `deye_device_{rated_power_watts,mppts,phases}` | inverter.* | | |
| `deye_running_status{status=...}` | inverter.run_status | one-hot | standby/selfcheck/normal/alarm/fault |
| `deye_tou_enabled`, `deye_tou_day{day=...}` | tou.* | | |
| `deye_tou_slot_{start_minutes,power_watts,voltage_volts,capacity_percent}{slot=1..6}` | tou.slots[] | | |
| `deye_tou_charge{slot,source=grid|gen}` | tou.slots[].{grid,gen}_charge | 0/1 | |
| `deye_scrape_success` | meta.scrape_success | 0/1 | exporter's own Modbus poll result |
| `deye_data_age_seconds` | meta.data_age_s | s | age of exporter data |
| `deye_last_successful_scrape_timestamp_seconds` | meta.last_success_unix_s | s | |

Informational duplicates (`deye_tou_slot_start_hhmm`, `deye_tou_slot_info`,
`deye_tou_mode`) are accepted but not used.

## Sign conventions

* **Battery power: positive = discharging.** Inferred from the live capture by
  energy balance: PV 2 W + grid 0 W + battery 738 W = 740 W against load
  729 W, at night with the grid disconnected. Encoded as a test
  (`test_energy_balance_sign`). One sample; re-confirm while charging.
* **Grid power: unverified.** The capture had the grid disconnected (0 W).
  The UI shows magnitude only and no flow arrow until the user selects the
  convention in *System -> Grid power sign*.

## Not available from the backend

* Generator-port measurements (voltage, frequency, power).
* Per-battery-module / BMS data.
* Inverter alarm/fault codes (only the one-hot running status).
* History. The exporter returns instantaneous samples; no Prometheus server
  was found (port 9090 closed). The HMI keeps a bounded volatile history.
* Any write / command endpoint.
