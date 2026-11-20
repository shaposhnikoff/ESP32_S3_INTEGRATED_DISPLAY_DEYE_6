/*
 * Board support for the Waveshare ESP32-S3-Touch-LCD-5B (1024x600).
 *
 * Pin map, RGB timings and the CH422G / GT911 reset sequence follow the
 * official Waveshare example (examples/ESP-IDF/09_lvgl_v9_demo,
 * Apache-2.0, https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5).
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_LCD_H_RES 1024
#define BOARD_LCD_V_RES 600

/*
 * Initialises I2C, the CH422G expander, the RGB panel, the GT911 touch
 * controller and the LVGL adapter, then starts the LVGL task and switches
 * the backlight on. All later LVGL access must hold esp_lv_adapter_lock().
 */
esp_err_t board_display_start(lv_display_t **out_disp);

/*
 * Backlight on/off through CH422G EXIO2. The 5B board exposes the backlight
 * only as an enable line, so there is no brightness (PWM) control.
 */
esp_err_t board_backlight_set(bool on);

#ifdef __cplusplus
}
#endif
