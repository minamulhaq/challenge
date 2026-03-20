# NOTE: This kind of mocking currently works on Linux targets only.
message(STATUS "building DRIVER MOCKS (GPIO only)")

idf_component_get_property(original_driver_dir driver COMPONENT_OVERRIDEN_DIR)

set(include_dirs
    "${original_driver_dir}/include"
    "${original_driver_dir}/include/driver"
    "${CMAKE_CURRENT_SOURCE_DIR}/../hal/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../soc/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../esp_hw_support/include")

idf_component_mock(INCLUDE_DIRS ${include_dirs}
    REQUIRES freertos
    MOCK_HEADER_FILES
    ${original_driver_dir}/include/driver/gpio.h)
