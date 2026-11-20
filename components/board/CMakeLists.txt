idf_component_register(
    SRCS "board_ws5b.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd
    PRIV_REQUIRES esp_timer)
