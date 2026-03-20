message(STATUS "building HW_I2C MOCKS")

idf_component_mock(
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    MOCK_HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_i2c.h
)
