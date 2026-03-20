idf_component_register(
    SRCS "ambient_light.c"
    INCLUDE_DIRS "."
    REQUIRES device_config hw_i2c
)
