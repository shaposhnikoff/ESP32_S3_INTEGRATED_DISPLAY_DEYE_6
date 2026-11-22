# Hardware compatibility

Target: **Waveshare ESP32-S3-Touch-LCD-7B** with touch, 1024x600. The previous
5B driver remains in the source tree but is not selected by this build.

| Signal | GPIO / expander | Source |
|---|---|---|
| R3..R7 | 1, 2, 42, 41, 40 | Waveshare docs + example |
| G2..G7 | 39, 0, 45, 48, 47, 21 | " |
| B3..B7 | 14, 38, 18, 17, 10 | " |
| PCLK / DE / HSYNC / VSYNC | 7 / 5 / 46 / 3 | " |
| I2C SDA / SCL | 8 / 9 | " |
| Touch INT (reset address strap) | 4 | example |
| LCD power, TP reset, brightness | CH422G output and brightness registers | 7B example |
| USB D-/D+ | 19 / 20 | docs |

Panel: 1024x600 RGB565, PCLK 24 MHz, horizontal sync 162/152/48 and vertical
sync 45/13/3, matching the existing 7B driver in `alarms_monitor` and the
[Waveshare 7B documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B).
Memory: the framebuffer count is chosen by
`esp_lv_adapter_get_required_frame_buffer_count`; partial rendering uses a
ten-line buffer in PSRAM. The 7B panel, touch controller and backlight were
initialized and the live Overview was captured from the connected board.

Power: USB-C 5 V on the 7B board.

Validation status: host UI and firmware build pass; the 7B was flashed, fetched
the live outage plan and produced a 1024x600 screenshot on 2026-11-22.
Long-running stability, static networking and OTA remain unverified. Previous
5B results remain in hardware-validation.md as historical evidence.
