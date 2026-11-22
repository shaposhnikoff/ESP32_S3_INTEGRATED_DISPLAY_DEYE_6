# Memory model

Measured by `idf.py size` (build of 2026-11-20):

| Region | Used | Free |
|---|---|---|
| DIRAM static | 156.6 KB (45.8 %) | 185 KB |
| App image | 1.61 MB | 62 % of 4 MB OTA slot |

Runtime allocations (bounded, with TLS transient allocations during requests):

| Buffer | Size | Location |
|---|---|---|
| framebuffers (x3, observed at boot) | 3 x 1.2 MB | PSRAM |
| RGB bounce buffer | 1024 x 20 x 2 B x 2 | internal |
| history (`history_t`) | ~68 KB | PSRAM |
| telemetry receive buffer | 64 KB (`CONFIG_DEYE_TELEMETRY_MAX_BYTES`) | PSRAM |
| outage JSON receive buffer | 128 KB maximum | PSRAM |
| outage task stack | 8 KB | PSRAM |
| LVGL task stack | 16 KB | PSRAM |
| snapshot buffer (only during a capture request) | 1.2 MB RGB565 | PSRAM |
| inverter model copies (state, parser scratch, UI snapshot) | ~1.5 KB each | static |

No per-poll heap allocation except what `esp_http_client` does internally.
Dialog contexts (`lv_malloc`) live only while a dialog is open. Chart query
buffers are static (240 points). The on-device history is intentionally
volatile (lost on reboot).

The 7B was measured on 2026-11-22. With all ten pages constructed, internal
heap free was about 8.5 KB and the minimum dropped to 251 bytes during a
screenshot. The first 8 KB outage task stack could not be created in internal
RAM. After moving that stack and TLS allocations to PSRAM, and constructing
only Overview, internal heap free was 35.6 KB and the minimum was 22.7 KB
after a successful 62,222-byte HTTPS fetch and screenshot. PSRAM free was
about 2.69 MB. `/metrics` exposes free, minimum and largest internal blocks,
PSRAM free/total, and outage fetch status. The RGB panel, Wi-Fi stacks and
buffers, LVGL objects and 64 KB reserved internal allocation pool account for
the small internal-RAM budget; the JSON file itself is stored in PSRAM.
