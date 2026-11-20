# Troubleshooting

| Symptom | Check |
|---|---|
| Status bar `SETUP` | Network -> Edit host/port |
| `WAITING` forever | Wi-Fi connected? Network -> Test connection; Last error |
| `OFFLINE`, error "Backend unreachable" | `curl http://<pi>:9101/metrics` from another machine; Pi firewall |
| `STALE`, "EXPORTER LOST INVERTER" | exporter's own Modbus poll fails (`deye_scrape_success 0`): RS485 adapter / inverter, not the display |
| "Telemetry incomplete or invalid" | exporter response lacks SOC or load power; Diagnostics -> Rejected / Malformed |
| Grid shows `?` | set System -> Grid power sign after confirming the convention |
| Generator shows `--` | expected: exporter exports no generator metrics |
| Charts empty after reboot | expected: history is volatile |
| Controls all disabled | expected in 0.1.0, see control-safety.md |
| Forgot PIN | factory reset requires access to System; otherwise `esptool erase_flash` and reflash |
| Screen dark | screen timeout: touch once to wake |
| Boot loop after OTA | automatic rollback after the crash; reflash via USB if both slots fail |
| Port not found when flashing | use the UART USB-C port; hold BOOT while plugging in |
