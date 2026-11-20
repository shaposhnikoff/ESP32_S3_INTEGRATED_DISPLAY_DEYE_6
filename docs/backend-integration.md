# Backend integration

## Decision

The existing exporter is reused unchanged. No gateway was added on the
Raspberry Pi: everything the HMI displays is available from `GET /metrics`,
and adding a gateway would not solve the missing write API (that needs the
backend owner's register documentation, see writable-parameters.md).

## Adapter

`components/app/telemetry.c` (transport) + `components/deye_core` (parsing,
model, health). The plan's `InverterBackend` interface maps as follows:

| Desired operation | Implementation |
|---|---|
| get_status / battery / pv / grid / load | one `GET /metrics` -> `im_parse_metrics()` -> `inverter_model_t` |
| get_generator | not exported; model fields stay invalid |
| get_history | not available from backend; on-device `history_t` |
| get_settings | ToU schedule from `/metrics` (read-only) |
| get_capabilities | `ctrl_capabilities_t`, `backend_write_api = false` |
| validate_command | `ctrl_validate()` |
| submit_command / get_command_status | no transport; lifecycle in `control.c` |

## Polling and failure handling

* Interval: configurable 2-30 s (default 3 s), changeable on the Network screen.
* HTTP timeout 5 s, redirects disabled, response limited to
  `CONFIG_DEYE_TELEMETRY_MAX_BYTES` (64 KB) in a PSRAM buffer.
* Reads are safe to retry: on failure the next poll uses exponential backoff
  2 s -> 60 s with 0-20 % jitter (`link_next_delay_ms`).
* The last good model is kept on failure; it is never shown as live
  (freshness below).
* "Test connection" and settings changes wake the poller immediately.

## Freshness model

`link_state.c` classifies data as:

| State | Condition |
|---|---|
| WAITING | no successful fetch since boot |
| LIVE | last fetch OK, exporter `scrape_success = 1`, data age <= stale threshold |
| STALE | data age unknown or above threshold, or exporter lost Modbus |
| OFFLINE | no successful fetch for 2x threshold |

The stale threshold defaults to 30 s. Non-live values are drawn grey and the
status bar shows the state as text and icon, not colour only.
Stale samples are excluded from history and command read-back verification.

## Running against the mock

```bash
cd backend/mock && uv run deye-mock --port 9101
# on the device: Network -> Edit host -> <your PC IP>
curl "http://<pc>:9101/_scenario?name=offline"
```
