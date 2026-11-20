# Memory model

Measured by `idf.py size` (build of 2026-11-20):

| Region | Used | Free |
|---|---|---|
| DIRAM static | 156.6 KB (45.8 %) | 185 KB |
| App image | 1.61 MB | 62 % of 4 MB OTA slot |

Runtime allocations (all bounded, allocated once at boot):

| Buffer | Size | Location |
|---|---|---|
| framebuffers (x3, observed at boot) | 3 x 1.2 MB | PSRAM |
| RGB bounce buffer | 1024 x 10 x 2 B x 2 | internal |
| history (`history_t`) | ~68 KB | PSRAM |
| telemetry receive buffer | 64 KB (`CONFIG_DEYE_TELEMETRY_MAX_BYTES`) | PSRAM |
| LVGL task stack | 16 KB | PSRAM |
| snapshot buffer (only during a capture request) | 1.2 MB RGB565 | PSRAM |
| inverter model copies (state, parser scratch, UI snapshot) | ~1.5 KB each | static |

No per-poll heap allocation except what `esp_http_client` does internally.
Dialog contexts (`lv_malloc`) live only while a dialog is open. Chart query
buffers are static (240 points). The on-device history is intentionally
volatile (lost on reboot).

Not measured: runtime heap watermark on hardware. The Diagnostics screen shows
free/min internal heap and PSRAM for that purpose.
