/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Derived from waveshare_rgb_lcd_port.c in
 * https://github.com/waveshareteam/ESP32-S3-Touch-LCD-5
 * (examples/ESP-IDF/09_lvgl_v9_demo), Copyright Waveshare, Apache-2.0.
 * Modifications: fixed to the 1024x600 (5B) panel, error propagation
 * instead of aborts, backlight off control, LVGL adapter start-up.
 */
#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "board";

#define I2C_PORT I2C_NUM_0
#define I2C_SDA 8
#define I2C_SCL 9
#define I2C_FREQ_HZ 400000
#define I2C_TIMEOUT_MS 100

/* CH422G register addresses (7-bit I2C addresses select the register). */
#define CH422G_REG_MODE 0x24
#define CH422G_REG_OUT 0x38
#define CH422G_MODE_OUTPUT 0x01
/* EXIO bits: 1 = TP_RST, 2 = DISP (backlight), 3 = LCD_RST, 4 = SD_CS */
#define EXIO_TP_RST (1u << 1)
#define EXIO_DISP (1u << 2)
#define EXIO_LCD_RST (1u << 3)
#define EXIO_SD_CS (1u << 4)

/* RGB timings for the 1024x600 panel, from the Waveshare example. */
#define LCD_PCLK_HZ (21 * 1000 * 1000)
#define LCD_BOUNCE_LINES 10

#define TOUCH_INT_GPIO GPIO_NUM_4 /* driven low during reset to select 0x5D */

static uint8_t s_exio = EXIO_TP_RST | EXIO_LCD_RST | EXIO_SD_CS;

static esp_err_t i2c_init(void)
{
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_PORT, &conf), TAG, "i2c config");
    esp_err_t err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    return (err == ESP_ERR_INVALID_STATE) ? ESP_OK : err;
}

static esp_err_t ch422g_write(uint8_t reg, uint8_t value)
{
    return i2c_master_write_to_device(I2C_PORT, reg, &value, 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t touch_reset(void)
{
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_REG_MODE, CH422G_MODE_OUTPUT), TAG, "ch422g mode");
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << TOUCH_INT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "touch int gpio");

    /* Same sequence as the vendor example: TP_RST low, INT low, TP_RST high. */
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_REG_OUT, 0x2C), TAG, "exio");
    esp_rom_delay_us(100 * 1000);
    gpio_set_level(TOUCH_INT_GPIO, 0);
    esp_rom_delay_us(100 * 1000);
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_REG_OUT, 0x2E), TAG, "exio");
    esp_rom_delay_us(200 * 1000);
    return ESP_OK;
}

esp_err_t board_backlight_set(bool on)
{
    if (on) {
        s_exio |= EXIO_DISP;
    } else {
        s_exio &= (uint8_t)~EXIO_DISP;
    }
    return ch422g_write(CH422G_REG_OUT, s_exio);
}

static esp_err_t panel_create(esp_lv_adapter_tear_avoid_mode_t tear, esp_lv_adapter_rotation_t rot,
                              esp_lcd_panel_handle_t *panel)
{
    const uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear, rot);
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings =
            {
                .pclk_hz = LCD_PCLK_HZ,
                .h_res = BOARD_LCD_H_RES,
                .v_res = BOARD_LCD_V_RES,
                .hsync_back_porch = 145,
                .hsync_front_porch = 170,
                .hsync_pulse_width = 30,
                .vsync_back_porch = 23,
                .vsync_front_porch = 12,
                .vsync_pulse_width = 2,
                .flags = {.pclk_active_neg = 1},
            },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = BOARD_LCD_H_RES * LCD_BOUNCE_LINES,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = -1,
        .data_gpio_nums =
            {
                GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_10,             /* B3..B7 */
                GPIO_NUM_39, GPIO_NUM_0, GPIO_NUM_45, GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, /* G2..G7 */
                GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,               /* R3..R7 */
            },
        .flags = {.fb_in_psram = 1},
    };
    ESP_LOGI(TAG, "RGB panel %dx%d, pclk %d Hz, %u frame buffers", BOARD_LCD_H_RES, BOARD_LCD_V_RES, LCD_PCLK_HZ,
             num_fbs);
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, panel), TAG, "rgb panel");
    return esp_lcd_panel_init(*panel);
}

static esp_err_t touch_create(esp_lcd_touch_handle_t *touch)
{
    ESP_RETURN_ON_ERROR(touch_reset(), TAG, "touch reset");
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 0; /* legacy I2C driver: bus speed set at install */
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_PORT, &io_cfg, &io), TAG, "touch io");
    const esp_lcd_touch_config_t cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
    };
    return esp_lcd_touch_new_i2c_gt911(io, &cfg, touch);
}

esp_err_t board_display_start(lv_display_t **out_disp)
{
    const esp_lv_adapter_rotation_t rot = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;

    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c");
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(panel_create(tear, rot, &panel), TAG, "panel");

    esp_lcd_touch_handle_t touch = NULL;
    esp_err_t terr = touch_create(&touch);
    if (terr != ESP_OK) {
        /* Keep the display usable for diagnostics even if touch fails. */
        ESP_LOGE(TAG, "touch init failed: %s", esp_err_to_name(terr));
        touch = NULL;
    }

    esp_lv_adapter_config_t acfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    acfg.task_stack_size = 16 * 1024;
    acfg.stack_in_psram = true;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&acfg), TAG, "lv adapter");

    esp_lv_adapter_display_config_t dcfg =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(panel, NULL, BOARD_LCD_H_RES, BOARD_LCD_V_RES, rot);
    dcfg.profile.use_psram = true;
    lv_display_t *disp = esp_lv_adapter_register_display(&dcfg);
    if (disp == NULL) {
        return ESP_FAIL;
    }
    if (touch != NULL) {
        esp_lv_adapter_touch_config_t tcfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch);
        if (esp_lv_adapter_register_touch(&tcfg) == NULL) {
            ESP_LOGE(TAG, "touch registration failed");
        }
    }
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "lv start");
    ESP_RETURN_ON_ERROR(board_backlight_set(true), TAG, "backlight");
    *out_disp = disp;
    return ESP_OK;
}
