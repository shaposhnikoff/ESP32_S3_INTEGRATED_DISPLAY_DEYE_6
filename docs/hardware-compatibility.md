# Hardware compatibility

Target: **Waveshare ESP32-S3-Touch-LCD-5B** only. The 800x480 "5" variant
needs different timings (`board_ws5b.c`) and is not supported by this build.

| Signal | GPIO / expander | Source |
|---|---|---|
| R3..R7 | 1, 2, 42, 41, 40 | Waveshare docs + example |
| G2..G7 | 39, 0, 45, 48, 47, 21 | " |
| B3..B7 | 14, 38, 18, 17, 10 | " |
| PCLK / DE / HSYNC / VSYNC | 7 / 5 / 46 / 3 | " |
| I2C SDA / SCL | 8 / 9 | " |
| Touch INT (reset address strap) | 4 | example |
| TP_RST, DISP, LCD_RST, SD_CS | CH422G EXIO1..4 | docs |
| USB D-/D+ | 19 / 20 | docs |

Panel: 1024x600 RGB565, PCLK 21 MHz, porches as in research.md.
Memory: the framebuffer count is chosen by
`esp_lv_adapter_get_required_frame_buffer_count`; the validated boot allocated
three RGB565 framebuffers of approximately 1.2 MB each in PSRAM.

Power: USB-C 5 V or 7-36 V DC terminal (board docs).

Validation status: **built, flashed and partially hardware-validated**.
Display/touch initialization, automatic Wi-Fi, live telemetry and actual
1024x600 device captures passed. Physical touch accuracy, flicker inspection
and long-duration testing remain pending. See hardware-validation.md.
