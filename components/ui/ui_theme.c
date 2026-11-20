#include "ui_internal.h"

ui_palette_t g_pal;

void ui_theme_init(lv_display_t *disp, bool dark)
{
    if (dark) {
        g_pal = (ui_palette_t){
            .bg = lv_color_hex(0x0E1318),
            .card = lv_color_hex(0x172029),
            .border = lv_color_hex(0x263241),
            .text = lv_color_hex(0xE6EDF3),
            .muted = lv_color_hex(0x8B98A5),
            .accent = lv_color_hex(0x3BA7FF),
            .ok = lv_color_hex(0x3FB950),
            .warn = lv_color_hex(0xD29922),
            .err = lv_color_hex(0xF85149),
            .pv = lv_color_hex(0xF2C14E),
            .battery = lv_color_hex(0x3FB950),
            .grid = lv_color_hex(0x58A6FF),
            .load = lv_color_hex(0xBC8CFF),
            .inactive = lv_color_hex(0x3A4654),
        };
    } else {
        g_pal = (ui_palette_t){
            .bg = lv_color_hex(0xEEF1F4),
            .card = lv_color_hex(0xFFFFFF),
            .border = lv_color_hex(0xD0D7DE),
            .text = lv_color_hex(0x1F2328),
            .muted = lv_color_hex(0x57606A),
            .accent = lv_color_hex(0x0969DA),
            .ok = lv_color_hex(0x1A7F37),
            .warn = lv_color_hex(0x9A6700),
            .err = lv_color_hex(0xCF222E),
            .pv = lv_color_hex(0xB8860B),
            .battery = lv_color_hex(0x1A7F37),
            .grid = lv_color_hex(0x0969DA),
            .load = lv_color_hex(0x8250DF),
            .inactive = lv_color_hex(0xAFB8C1),
        };
    }
    lv_theme_t *th = lv_theme_default_init(disp, g_pal.accent, g_pal.warn, dark, &lv_font_montserrat_16);
    lv_display_set_theme(disp, th);
}
