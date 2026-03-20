#ifndef TEMPERATURE_SENSOR_H_
#define TEMPERATURE_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef void (*TemperatureCallback_t)(int16_t wTemperature);

/**
 * @brief Initialize the temperature sensor component
 *
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t temperatureSensorInit(void);

/**
 * @brief Read the last measured temperature value
 *
 * @param pTemperature - pointer to store temperature in 0.1°C units
 * @return ESP_OK if data is available, ESP_ERR_NOT_FOUND if no reading yet
 */
esp_err_t temperatureSensorRead(int16_t *pTemperature);

/**
 * @brief Register a callback for new temperature readings
 *
 * @param cbCallback - callback function, or NULL to unregister
 */
void temperatureSensorRegisterCallback(TemperatureCallback_t cbCallback);

/**
 * @brief Put the sensor into low-power standby mode
 *
 * @return ESP_OK on success
 */
esp_err_t temperatureSensorSleep(void);

/**
 * @brief Wake the sensor from standby mode
 *
 * @return ESP_OK on success
 */
esp_err_t temperatureSensorWake(void);

#ifdef PROJECT_UNIT_TESTS
void temperatureSensorTestSetUp(void);
#endif

#endif /* TEMPERATURE_SENSOR_H_ */
