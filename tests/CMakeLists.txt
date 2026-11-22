# Host-side unit tests for the platform-independent core.
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.16)
project(deye_hmi_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../components/deye_core deye_core)
target_compile_definitions(deye_core PRIVATE DEYE_CORE_TESTING=1)

option(DEYE_SANITIZE "Build tests with ASan/UBSan" ON)
if(DEYE_SANITIZE)
    target_compile_options(deye_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(deye_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(core_tests
    unit/test_main.c unit/test_prom_parser.c unit/test_inverter_model.c
    unit/test_link_state.c unit/test_history.c unit/test_control.c unit/test_fmt.c
    unit/test_outage_dial.c unit/test_outage_json.c)
target_link_libraries(core_tests PRIVATE deye_core)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)
if(DEYE_SANITIZE)
    target_compile_options(core_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
target_compile_definitions(core_tests PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

enable_testing()
add_test(NAME core_tests COMMAND core_tests)

# Loaded by backend/mock tests through ctypes. Compile the same core sources
# without ASan here: Python does not preload the sanitizer runtime.
add_library(telemetry_probe SHARED
    integration/telemetry_probe.c
    ../components/deye_core/src/prom_parser.c
    ../components/deye_core/src/inverter_model.c
    ../components/deye_core/src/link_state.c)
target_include_directories(telemetry_probe PRIVATE ../components/deye_core/include)
target_compile_options(telemetry_probe PRIVATE -Wall -Wextra -Werror)
set_target_properties(telemetry_probe PROPERTIES SUFFIX ".so")
