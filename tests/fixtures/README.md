# Test fixtures

| File | Origin |
|---|---|
| `metrics_live_2026-11-20.txt` | Real capture of `GET http://192.168.10.39:9101/metrics` on 2026-11-20 (grid disconnected, battery discharging ~738 W, night). |
| `health_live_2026-11-20.json` | Real capture of `GET /health` from the same exporter. |
| `metrics_alarm.txt` | Live capture with `deye_running_status` switched from `normal` to `alarm`. |
| `metrics_backend_stale.txt` | Live capture with `deye_scrape_success 0` and `deye_data_age_seconds 600` (exporter lost Modbus). |
| `metrics_soc75.txt` | Live capture with SOC changed 94 -> 75 %. |
| `metrics_missing_core.txt` | Live capture without SOC and load power (must be rejected as telemetry). |
| `metrics_invalid_values.txt` | Synthetic corrupted values and malformed lines. |

Derived fixtures are edits of the real capture; they are simulations, not observed inverter states.

All real serial labels are replaced with `TEST000001` for publication.
