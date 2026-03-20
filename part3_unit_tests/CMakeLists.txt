cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main)
set(COMPONENT_NAME "ambient_light")
set(CHALLENGE_DIR "${CMAKE_SOURCE_DIR}/..")

add_definitions(-include "${CHALLENGE_DIR}/test_support/unit_tests_definitions.h")

list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/freertos")
list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/driver")

list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/device_config")
list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/hw_i2c")

project(unit_tests)
