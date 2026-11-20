/* Only the platform boundary is faked; UI and LVGL are compiled unchanged. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define portMAX_DELAY UINT32_MAX
const char *esp_err_to_name(esp_err_t error);
size_t strlcpy(char *dst, const char *src, size_t size);
