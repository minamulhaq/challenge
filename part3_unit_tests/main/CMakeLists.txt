set(component_dir "${CMAKE_SOURCE_DIR}/../part2_new_component")

file(GLOB component_srcs
     "${component_dir}/*.c"
)

idf_component_register(
    SRCS "main.c" "${component_srcs}"
    INCLUDE_DIRS "." "${component_dir}"
    REQUIRES cmock driver device_config hw_i2c
)

target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
target_link_libraries(${COMPONENT_LIB} --coverage)
