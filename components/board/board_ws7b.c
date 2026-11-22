/* Waveshare ESP32-S3-Touch-LCD-7B panel, touch and backlight. */
#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_rom_sys.h"

static const char *TAG = "board";

#define CH422G_MODE 0x02
#define CH422G_OUTPUT 0x03
#define CH422G_BRIGHTNESS 0x05

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_expander;

static esp_err_t ch422g_write(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(s_expander, bytes, sizeof(bytes), 100);
}

esp_err_t board_backlight_set(bool on)
{
    return ch422g_write(CH422G_BRIGHTNESS, on ? 0 : 255);
}

static esp_err_t i2c_start(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_8,
        .scl_io_num = GPIO_NUM_9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &s_i2c_bus), TAG, "i2c bus");
    const i2c_device_config_t device = {.device_address = 0x24, .scl_speed_hz = 400000};
    return i2c_master_bus_add_device(s_i2c_bus, &device, &s_expander);
}

static esp_err_t touch_reset(void)
{
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_MODE, 0xff), TAG, "expander mode");
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_OUTPUT, 0xdf), TAG, "lcd power");
    const gpio_config_t irq = {.pin_bit_mask = 1ULL << GPIO_NUM_4, .mode = GPIO_MODE_INPUT_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&irq), TAG, "touch irq");
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_OUTPUT, 0xdd), TAG, "touch reset low");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_4, 0), TAG, "touch address");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_OUTPUT, 0xdf), TAG, "touch reset high");
    esp_rom_delay_us(200000);
    return ESP_OK;
}

static esp_err_t panel_start(esp_lcd_panel_handle_t *panel)
{
    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const uint8_t buffers =
        esp_lv_adapter_get_required_frame_buffer_count(ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB, rotation);
    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings =
            {
                .pclk_hz = 24000000,
                .h_res = BOARD_LCD_H_RES,
                .v_res = BOARD_LCD_V_RES,
                .hsync_pulse_width = 162,
                .hsync_back_porch = 152,
                .hsync_front_porch = 48,
                .vsync_pulse_width = 45,
                .vsync_back_porch = 13,
                .vsync_front_porch = 3,
                .flags.pclk_active_neg = true,
            },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = buffers,
        .bounce_buffer_size_px = BOARD_LCD_H_RES * 20,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
        .flags.fb_in_psram = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, panel), TAG, "rgb panel");
    return esp_lcd_panel_init(*panel);
}

esp_err_t board_display_start(lv_display_t **out_disp)
{
    ESP_RETURN_ON_ERROR(i2c_start(), TAG, "i2c");
    ESP_RETURN_ON_ERROR(touch_reset(), TAG, "touch reset");
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(panel_start(&panel), TAG, "panel");

    esp_lcd_touch_handle_t touch = NULL;
    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz = 400000;
    esp_err_t touch_err = esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &touch_io);
    if (touch_err == ESP_OK) {
        const esp_lcd_touch_config_t touch_config = {
            .x_max = BOARD_LCD_H_RES,
            .y_max = BOARD_LCD_V_RES,
            .rst_gpio_num = -1,
            .int_gpio_num = GPIO_NUM_4,
            .levels = {.reset = 0, .interrupt = 0},
        };
        touch_err = esp_lcd_touch_new_i2c_gt911(touch_io, &touch_config, &touch);
    }
    if (touch_err != ESP_OK) {
        ESP_LOGE(TAG, "touch init failed: %s", esp_err_to_name(touch_err));
    }

    esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter.task_stack_size = 16 * 1024;
    adapter.stack_in_psram = true;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter), TAG, "LVGL adapter");
    esp_lv_adapter_display_config_t display = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel, NULL, BOARD_LCD_H_RES, BOARD_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    display.profile.buffer_height = 10;
    display.profile.use_psram = true;
    lv_display_t *lv_display = esp_lv_adapter_register_display(&display);
    if (lv_display == NULL)
        return ESP_ERR_NO_MEM;
    if (touch != NULL) {
        const esp_lv_adapter_touch_config_t input = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(lv_display, touch);
        if (esp_lv_adapter_register_touch(&input) == NULL)
            ESP_LOGE(TAG, "touch registration failed");
    }
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "LVGL start");
    ESP_RETURN_ON_ERROR(board_backlight_set(true), TAG, "backlight");
    *out_disp = lv_display;
    return ESP_OK;
}
