# Research notes

Legend: **[V]** verified from primary source or measurement in this project,
**[A]** assumption, **[U]** unverified.

## Board: Waveshare ESP32-S3-Touch-LCD-5B (SKU 28151)

* [V] ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB octal PSRAM (Waveshare docs).
* [V] 5B variant = 1024x600 panel; the 5 (non-B) variant is 800x480. Both share
  the PCB and pin map (docs page lists both resolutions).
* [V] RGB565 16-bit parallel interface; pin map in hardware-compatibility.md.
* [V] Touch GT911 on I2C0 (SDA 8, SCL 9), address 0x5D; CH422G expander at
  0x24 (mode) / 0x38 (outputs) controls TP_RST, DISP (backlight), LCD_RST,
  SD_CS.
* [V] 1024x600 timings from the official ESP-IDF example
  `examples/ESP-IDF/09_lvgl_v9_demo`: PCLK 21 MHz, HSYNC bp/fp/pw 145/170/30,
  VSYNC bp/fp/pw 23/12/2, pclk active negative, bounce buffer 10 lines,
  framebuffers in PSRAM.
* [V] Example requires ESP-IDF >= 5.5.0, LVGL ^9, `espressif/esp_lvgl_adapter`
  ^0.5.2, `esp_lcd_touch_gt911` ^1.
* [V] Example sdkconfig: PSRAM octal 80 MHz, `SPIRAM_XIP_FROM_PSRAM`, 32 KB
  I-cache, 64 KB D-cache with 64 B lines. Adopted unchanged.
* [V] Backlight is a single enable line (EXIO2) -> no brightness control.
* [U] Flicker at 21 MHz PCLK under heavy PSRAM load (Wi-Fi + rendering).
  Mitigations adopted from the vendor: bounce buffers, XIP from PSRAM,
  `LCD_RGB_ISR_IRAM_SAFE`. Needs hardware observation.

## Reference projects

| Repository | Use | License note |
|---|---|---|
| waveshareteam/ESP32-S3-Touch-LCD-5 | board init adapted in `components/board` | Apache-2.0, attribution kept |
| tuanchris/dune-weaver-touch | not used (different stack) | - |
| esp-arduino-libs/ESP32_Display_Panel | not used: Arduino-oriented; ESP-IDF adapter chosen | - |
| DocBigs-Lab/CYD-Solar-Display, scrhall/deye-sun6k-waveshare-esphome, VMrenato/homeassistant-deye-tcan485-esphome | not used: they talk Modbus directly, which this project must not do | no code copied |

No third-party source was copied except the Apache-2.0 Waveshare board code.

## Backend

* [V] Exporter at 192.168.10.39:9101 serves `/metrics` and `/health` (see
  api-contract.md). [V] No write API on common ports.
* [V] Battery power + = discharge (one-sample energy balance). [U] Grid sign.
* [V] No Prometheus server on 9090 -> no backend history.

## Inverter SUN-6K-SG05LP1-EU-AM2-P

* [V] From telemetry: rated 6000 W, 2 MPPT, 1 phase, ToU with 6 slots.
* [U] Modbus register persistence (EEPROM/flash) of writable settings:
  unknown; treated as non-volatile (worst case) in control-safety.md.
