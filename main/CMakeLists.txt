idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES board app ui
    PRIV_REQUIRES esp_timer)
