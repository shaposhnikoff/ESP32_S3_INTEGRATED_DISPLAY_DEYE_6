#include "diag.h"

#include <string.h>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"

static const char *reset_reason_text(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:
        return "Power-on";
    case ESP_RST_SW:
        return "Software restart";
    case ESP_RST_PANIC:
        return "Panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "Watchdog";
    case ESP_RST_BROWNOUT:
        return "Brownout";
    case ESP_RST_EXT:
        return "External";
    default:
        return "Other";
    }
}

void diag_read(diag_info_t *out)
{
    const esp_app_desc_t *d = esp_app_get_description();
    strlcpy(out->fw_version, d->version, sizeof(out->fw_version));
    strlcpy(out->idf_version, d->idf_ver, sizeof(out->idf_version));
    snprintf(out->build_date, sizeof(out->build_date), "%s", d->date);
    const esp_partition_t *p = esp_ota_get_running_partition();
    strlcpy(out->partition, p ? p->label : "?", sizeof(out->partition));
    out->uptime_s = (uint64_t)(esp_timer_get_time() / 1000000);
    out->heap_internal_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->heap_internal_min = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    out->psram_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->psram_total = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    out->reset_reason = reset_reason_text(esp_reset_reason());
}
