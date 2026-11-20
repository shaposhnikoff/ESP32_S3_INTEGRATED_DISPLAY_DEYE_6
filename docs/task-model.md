# Task model

| Task / context | Priority | Stack | Blocking allowed | Notes |
|---|---|---|---|---|
| LVGL (esp_lvgl_adapter) | adapter default | 16 KB (PSRAM) | no network, short I2C only | all `lv_*` calls; UI timers and event callbacks |
| telemetry | 4 | 8 KB | yes, bounded by 5 s HTTP timeout | sleeps on task notification for the poll interval |
| ota (on demand) | 3 | 8 KB | yes | created only after user confirmation, deletes itself |
| esp_timer callbacks | system | - | no | Wi-Fi retry, RSSI refresh, app-valid marker |
| Wi-Fi/event loop | system | - | no | `net_wifi.c` event handler updates state under lock |

Inter-task communication: shared state under mutex + `xTaskNotifyGive` to wake
the poller. No queues are needed because the UI is pull-based (snapshot).

Watchdog: task WDT 10 s with panic (sdkconfig). The telemetry task is not
subscribed because its HTTP calls may legitimately take > 10 s in total; all
its blocking calls are timeout-bounded instead.

`ui_main.c` calls from event callbacks into `app_settings_*`, `settings_save`
(NVS write, short) and non-blocking starters (`net_wifi_reconnect`,
`telemetry_poll_now`, `ota_start`).
