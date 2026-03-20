# NOTE: This kind of mocking currently works on Linux targets only.
message(STATUS "building FREERTOS MOCKS (task, event-groups, queue, timers)")

idf_component_get_property(original_freertos_dir freertos COMPONENT_OVERRIDEN_DIR)

set(include_dirs
    "include"
    "${original_freertos_dir}/FreeRTOS-Kernel/include"
    "${original_freertos_dir}/esp_additions/include"
    "${original_freertos_dir}/esp_additions/include/freertos"
    "${original_freertos_dir}/FreeRTOS-Kernel/include/freertos"
    "${original_freertos_dir}/FreeRTOS-Kernel/portable/linux/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../soc/include")

idf_component_mock(INCLUDE_DIRS ${include_dirs}
    REQUIRES esp_common
    MOCK_HEADER_FILES
    ${original_freertos_dir}/FreeRTOS-Kernel/include/freertos/task.h
    ${original_freertos_dir}/FreeRTOS-Kernel/include/freertos/event_groups.h
    ${original_freertos_dir}/FreeRTOS-Kernel/include/freertos/queue.h
    ${original_freertos_dir}/FreeRTOS-Kernel/include/freertos/timers.h
)
