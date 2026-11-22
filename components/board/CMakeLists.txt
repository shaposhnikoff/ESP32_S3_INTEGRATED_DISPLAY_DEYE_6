idf_component_register(
    SRCS "board_ws7b.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd
    PRIV_REQUIRES esp_timer)
