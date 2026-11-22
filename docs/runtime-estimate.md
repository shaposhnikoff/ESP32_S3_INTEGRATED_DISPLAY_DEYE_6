# Battery time estimate

The Overview uses the configured combined battery capacity of **15,000 Wh**
(three approximately 5 kWh batteries) and a **20% cutoff**. These defaults are
kept in display settings (`bat_wh`, `bat_cut`) and are used when older NVS
profiles have no values for those keys. The Prometheus exporter supplies SOC
and signed battery power but does not publish pack capacity or shutdown SOC.
Its `deye_tou_slot_capacity_percent` is a Time of Use target for each slot,
not the battery's physical capacity or global cutoff.

The 7B Overview estimate is
`(SOC - 20) / 100 * 15000 Wh / max(5-minute average house load W, 50 W)`.
It uses recent one-minute history buckets when available, and the current
measured load on startup. It is unavailable when telemetry or load is missing,
or the inverter reports a fault. At or below the cutoff, runtime is zero.
The schedule coverage chip requires runtime to exceed the outage duration by
15 minutes; it is hidden while schedule data is unavailable.

The asterisk on `RUNTIME*` marks an estimate.
Battery capacity is approximate; effective capacity changes with temperature,
age and BMS limits. Future power use or charge rate may differ from the
current sample. No inverter or Modbus settings are changed by this feature.
