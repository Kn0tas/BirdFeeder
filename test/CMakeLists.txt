cmake_minimum_required(VERSION 3.16)
project(birdfeeder_tests C)

# Unity test framework
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Path to main/ headers (extracted logic only)
set(MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../main")

# Test executable
add_executable(test_runner
    test_main.c
    test_servo_math.c
    test_vision_utils.c
    test_threat_logic.c
    test_sensor_conversions.c
    test_fram_bounds.c
)

target_include_directories(test_runner PRIVATE
    ${MAIN_DIR}
    ${MAIN_DIR}/actuators
    ${MAIN_DIR}/sensors
    ${MAIN_DIR}/vision
)

target_link_libraries(test_runner PRIVATE unity)
