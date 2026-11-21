# Battery time estimate

The Overview uses the configured combined battery capacity of **15,000 Wh**
(three approximately 5 kWh batteries) and a **20% cutoff**. These defaults are
kept in display settings (`bat_wh`, `bat_cut`) and are used when older NVS
profiles have no values for those keys. The Prometheus exporter supplies SOC
and signed battery power but does not publish pack capacity or shutdown SOC.
Its `deye_tou_slot_capacity_percent` is a Time of Use target for each slot,
not the battery's physical capacity or global cutoff.

While the battery discharges, the estimate is
`(SOC - 20) / 100 * 15000 Wh / battery power W`. While it charges, the
estimate to full is `(100 - SOC) / 100 * 15000 Wh / |battery power W|`.
The result is rounded to minutes. It is unavailable when telemetry is not
live, inputs are invalid, battery power is within 20 W of idle, or the inverter
reports a fault. At or below the cutoff, discharge runtime is zero.

The asterisk on `RUNTIME*` and `FULL IN*` marks an instantaneous estimate.
Battery capacity is approximate; effective capacity changes with temperature,
age and BMS limits. Future power use or charge rate may differ from the
current sample. No inverter or Modbus settings are changed by this feature.
