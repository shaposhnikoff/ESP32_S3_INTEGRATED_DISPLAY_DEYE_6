# Technical decisions

| Topic | Decision | Reason |
|---|---|---|
| Framework | ESP-IDF 5.5.1, C11, no Arduino | vendor example targets IDF >= 5.5; native HTTP/NVS/OTA |
| Toolchain | project Dockerfile based on `espressif/idf:v5.5.1` | identical local/CI SDK filter preserves pinned registry dependencies on configure reruns |
| GUI | LVGL 9.3.0 via `esp_lvgl_adapter` 0.5.2 | matches the verified vendor example (tear avoidance, RGB bounce buffers) |
| Board code | adapted from Waveshare Apache-2.0 example | verified timings; no guessing |
| Backend | reuse exporter `/metrics` read-only | sufficient for all display features; no gateway needed |
| Parsing | own allocation-free Prometheus parser | tiny, bounded, host-testable |
| Core logic | `deye_core` without ESP dependencies | runs host unit tests with sanitizers |
| History | on-device bounded ring (24 h @1 min, 7 d @15 min, ~68 KB PSRAM) | backend has no history store |
| Control | full safety lifecycle, writes disabled | no write API and no verified registers |
| Credentials | own NVS namespace, Wi-Fi driver storage RAM-only | single place to erase; never displayed |
| OTA | HTTPS + CA bundle, rollback enabled, HTTP only via Kconfig opt-in | no silent downgrade |

ADRs with context: `docs/adr/`.
