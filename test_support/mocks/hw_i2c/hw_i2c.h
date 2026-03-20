#ifndef HW_I2C_H_
#define HW_I2C_H_

#include "esp_err.h"

/**
 * @brief Initialize I2C communication
 *
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t i2cInit(void);

/**
 * @brief Write bytes to I2C slave device
 *
 * @param ubAddr - device address (7-bit)
 * @param pVal - pointer to buffer with bytes to write
 * @param ulSize - size of buffer
 *
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t i2cWrite(uint8_t ubAddr, uint8_t *pVal, uint32_t ulSize);

/**
 * @brief Read bytes from I2C slave device register
 *
 * @param ubAddr - device address (7-bit)
 * @param ubReg - 8-bit register to read
 * @param pVal - pointer to buffer for read data
 * @param ulSize - number of bytes to read
 *
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t i2cRead(uint8_t ubAddr, uint8_t ubReg, uint8_t *pVal, uint32_t ulSize);

#endif /* HW_I2C_H_ */
