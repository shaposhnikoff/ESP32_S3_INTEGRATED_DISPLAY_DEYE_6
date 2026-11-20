# ADR 0001: ESP-IDF 5.5.1 + LVGL 9.3 via esp_lvgl_adapter

Status: accepted (2026-11-20)

Context: the only vendor-verified 1024x600 initialisation for this board is
the Waveshare ESP-IDF LVGL 9 example, which uses `esp_lvgl_adapter` and needs
IDF >= 5.5.

Decision: pin IDF 5.5.1 (Docker image), LVGL 9.3.0, adapter 0.5.2, GT911 1.1.3;
keep `dependencies.lock` committed. No Arduino.

Consequences: adapter API (lock/unlock, tear-avoid modes) is a dependency;
upgrades require re-checking the vendor example.
