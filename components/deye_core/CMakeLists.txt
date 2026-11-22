# Platform-independent core. Built as an ESP-IDF component in firmware
# builds and as a plain static library by tests/CMakeLists.txt.
if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/prom_parser.c" "src/inverter_model.c" "src/link_state.c"
             "src/history.c" "src/control.c" "src/fmt.c" "src/outage_dial.c" "src/outage_json.c"
        INCLUDE_DIRS "include")
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
else()
    add_library(deye_core STATIC
        src/prom_parser.c src/inverter_model.c src/link_state.c
        src/history.c src/control.c src/fmt.c src/outage_dial.c src/outage_json.c)
    target_include_directories(deye_core PUBLIC include)
    target_compile_options(deye_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(deye_core PUBLIC m)
endif()
