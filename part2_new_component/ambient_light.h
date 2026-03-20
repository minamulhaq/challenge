#ifndef AMBIENT_LIGHT_H_
#define AMBIENT_LIGHT_H_

#include <stdint.h>
#include "esp_err.h"

typedef void (*AmbientLightCallback_t)(uint16_t uwLux);

/**
 * @brief Initialize the ambient light sensor component
 *
 * Configures GPIO interrupt on ALERT pin, creates binary semaphore and
 * processing task, sets sensor to active mode with interrupt enabled,
 * sets default upper threshold to 1000 lux, clears any pending interrupt.
 * Must be called after gpio_install_isr_service().
 *
 * @return ESP_OK on success, ESP_FAIL if any critical step fails
 */
esp_err_t ambientLightInit(void);

/**
 * @brief Read current ambient light level in lux
 *
 * @param pLux - pointer to store the lux reading
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if pLux is NULL,
 *         otherwise I2C error code
 */
esp_err_t ambientLightRead(uint16_t *pLux);

/**
 * @brief Write upper threshold register
 *
 * Alert fires when reading exceeds this threshold.
 *
 * @param uwThreshold - upper threshold in lux
 *
 * @return ESP_OK on success, otherwise I2C error code
 */
esp_err_t ambientLightSetThreshold(uint16_t uwThreshold);

/**
 * @brief Put sensor in standby mode
 *
 * Writes 0x00 to CONTROL register.
 *
 * @return ESP_OK on success, otherwise I2C error code
 */
esp_err_t ambientLightSleep(void);

/**
 * @brief Put sensor in active (continuous) mode
 *
 * Writes 0x01 to CONTROL register.
 *
 * @return ESP_OK on success, otherwise I2C error code
 */
esp_err_t ambientLightWake(void);

/**
 * @brief Register or unregister a callback for threshold alerts
 *
 * @param cbCallback - callback invoked with lux value on alert, or NULL to unregister
 */
void ambientLightRegisterCallback(AmbientLightCallback_t cbCallback);

#ifdef PROJECT_UNIT_TESTS
void ambientLightTestSetUp(void);
#endif

#endif /* AMBIENT_LIGHT_H_ */
