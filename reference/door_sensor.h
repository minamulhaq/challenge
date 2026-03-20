#ifndef DOOR_SENSOR_H_
#define DOOR_SENSOR_H_

#include <stdbool.h>
#include "esp_err.h"

typedef void (*DoorSensorCallback_t)(bool isDoorOpen);

/**
 * @brief Check if the door is currently open
 *
 * @return true if door is open, false if closed
 */
bool doorSensorIsOpen(void);

/**
 * @brief Register a callback for door state changes
 *
 * @param cbCallback - callback function invoked on state change, or NULL to unregister
 */
void doorSensorRegisterCallback(DoorSensorCallback_t cbCallback);

/**
 * @brief Initialize the door sensor component
 *
 * Configures GPIO with interrupt, creates binary semaphore and processing task.
 * Must be called after gpio_install_isr_service().
 *
 * @return ESP_OK on success, ESP_FAIL if task creation fails
 */
esp_err_t doorSensorInit(void);

#ifdef PROJECT_UNIT_TESTS
void doorSensorTestSetUp(void);
#endif

#endif /* DOOR_SENSOR_H_ */
