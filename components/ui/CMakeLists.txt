idf_component_register(
    SRCS "ui_main.c" "ui_theme.c" "ui_widgets.c" "ui_capture_http.c" "i18n.c"
         "scr_overview.c" "scr_battery.c" "scr_solar.c" "scr_grid.c" "scr_generator.c"
         "scr_charts.c" "scr_controls.c" "scr_network.c" "scr_diag.c" "scr_system.c"
         "fonts/deye_font_status.c" "fonts/deye_font_inverter.c" "fonts/deye_font_soc.c"
         "fonts/deye_font_value.c" "fonts/deye_font_tile.c" "fonts/deye_font_data.c"
         "fonts/deye_font_dial_soc.c" "fonts/deye_font_dial_time.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl
    PRIV_REQUIRES app board deye_core esp_system esp_http_server esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wno-missing-field-initializers)
