# Architecture

```text
Raspberry Pi (unchanged)                ESP32-S3-Touch-LCD-7B
+---------------------------+  Wi-Fi   +------------------------------------------+
| Deye exporter :9101       | <------- | telemetry task: HTTP GET /metrics        |
|  Modbus RS485 -> inverter |  GET     |   -> deye_core parser -> inverter_model  |
+---------------------------+          |   -> app_state (mutex) + history         |
Public Kyiv-region JSON -- HTTPS GET ->| outage task -> validated group 4.1 plan |
                                       |   -> app_state (mutex)                    |
                                       | LVGL task: 500 ms timer copies state     |
                                       |   -> Overview updates labels and dial    |
                                       +------------------------------------------+
```

The RS485 link stays owned by the Raspberry Pi; the display never talks
Modbus and only issues HTTP GET.

## Components

| Component | Role | ESP deps |
|---|---|---|
| `components/deye_core` | Prometheus parser, typed model, history, outage dial and coverage logic | none (host-testable) |
| `components/board` | 7B RGB panel, CH422G, GT911, LVGL adapter start, backlight | esp_lcd, driver |
| `components/app` | settings (NVS), app_state, Wi-Fi, telemetry and outage tasks, OTA, diagnostics | IDF |
| `components/ui` | theme, widgets and the single-screen Overview | LVGL |
| `main` | boot sequence | - |

## Data flow and ownership

* `app_state.c` owns the model, link state, Wi-Fi info, OTA info, history and
  control context behind one mutex; settings behind a second small mutex.
  Lock order: never hold both (snapshot takes them sequentially).
* Producers parse **outside** the lock and copy the result in.
* The UI never blocks: `app_state_snapshot()` uses try-lock; on contention the
  refresh is skipped for one tick.
* Only Overview is created at boot. Updates change labels and arc angles in
  place (no per-update object churn).

## Boot

settings -> state -> display/touch/LVGL -> Overview -> Wi-Fi -> telemetry and
outage reader. The
control context starts empty on every boot; nothing is replayed. After 60 s of
healthy running the OTA image is marked valid (rollback otherwise).

See also: task-model.md, memory-model.md, network-architecture.md,
control-safety.md, `docs/diagrams/`.
