#include "temperature_sensor.h"
#include "device_config.h"
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include <driver/i2c.h>

#define TEMP_SENSOR_I2C_ADDR		0x48
#define TEMP_REG_DATA				0x00
#define TEMP_REG_CONFIG				0x01
#define TEMP_REG_ALERT_HIGH			0x02
#define TEMP_REG_ALERT_LOW			0x03
#define TEMP_CONFIG_STANDBY			0x01
#define TEMP_CONFIG_ACTIVE			0x00

#define DEBOUNCE_TIME_MS			30

char TAG[] = "temp_sensor";

static TaskHandle_t s_pTaskHandler = NULL;
static SemaphoreHandle_t s_pSemaphoreHandle = NULL;
static StaticSemaphore_t s_sSemaphoreBuffer;
static TemperatureCallback_t s_cbTempCallback = NULL;
static int16_t s_wLastReading = 0;
static bool s_isDataReady = false;


static void s_tempAlertIntrEvent(void *pArg) {
	BaseType_t ulHigherPriorityTaskWoken = pdFALSE;
	gpio_isr_handler_remove(CONFIG_PIN_TEMP_ALERT);
	xSemaphoreGiveFromISR(s_pSemaphoreHandle, &ulHigherPriorityTaskWoken);
	portYIELD_FROM_ISR();
}


static void s_configureGpio(void)
{
	gpio_config_t sGpioConfig = {
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = (1ULL << CONFIG_PIN_TEMP_ALERT),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_NEGEDGE,
	};
	gpio_config(&sGpioConfig);
	gpio_isr_handler_add(CONFIG_PIN_TEMP_ALERT, s_tempAlertIntrEvent, NULL);
}


static esp_err_t s_readTemperature(int16_t *pTemperature)
{
	uint8_t data[2] = {0};

	esp_err_t lErr = i2c_master_write_read_device(
		I2C_NUM_1, TEMP_SENSOR_I2C_ADDR, (uint8_t[]){TEMP_REG_DATA}, 1,
		data, 2, pdMS_TO_TICKS(100));

	if (lErr == ESP_OK) {
		int temperature = ((int16_t)data[0] << 8) | data[1];
		*pTemperature = temperature;
	}

	return lErr;
}


static esp_err_t s_writeConfig(uint8_t ubValue)
{
	uint8_t pTxBuff[2] = {TEMP_REG_CONFIG, ubValue};
	return i2c_master_write_to_device(
		I2C_NUM_1, TEMP_SENSOR_I2C_ADDR, pTxBuff, 2, pdMS_TO_TICKS(100));
}


static void s_tempSensorTask(void *pArgs)
{
	#ifdef PROJECT_UNIT_TESTS
		bool isInfiniteLoop = false;
	#else
		bool isInfiniteLoop = true;
	#endif

	do {
		xSemaphoreTake(s_pSemaphoreHandle, portMAX_DELAY);
		ESP_LOGI(TAG, "alert triggered");

		vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));

		int16_t wReading = 0;
		esp_err_t lErr = s_readTemperature(&wReading);

		if (lErr != ESP_OK) {
			ESP_LOGE(TAG, "read error: 0x%X", lErr);
			goto read_done;
		}

		float result = (float)wReading / 256.0f;
		ESP_LOGI(TAG, "temperature: %.1f C", result);

		s_wLastReading = wReading;
		s_isDataReady = true;

		if (NULL != s_cbTempCallback) {
			s_cbTempCallback(wReading);
		}

		char *pAlertMsg = (char *)malloc(48);
		if (pAlertMsg != NULL) {
			snprintf(pAlertMsg, 48, "temp_alert:raw=%d,c=%.1f", wReading, result);
		}

		if (5000 < wReading || -1000 > wReading) {
			ESP_LOGW(TAG, "%s", pAlertMsg);
			free(pAlertMsg);
		}

read_done:
		gpio_isr_handler_add(CONFIG_PIN_TEMP_ALERT, s_tempAlertIntrEvent, NULL);
	} while (isInfiniteLoop);

	s_pTaskHandler = NULL;
	vTaskDelete(NULL);
}


esp_err_t temperatureSensorInit() {
	s_pSemaphoreHandle = xSemaphoreCreateBinaryStatic(&s_sSemaphoreBuffer);

	s_configureGpio();

	if (!xTaskCreatePinnedToCore(s_tempSensorTask, "temp_sensor", 1024,
			NULL, 5, &s_pTaskHandler, 1)) {
		ESP_LOGE(TAG, "Error starting task");
		return ESP_FAIL;
	}

	return ESP_OK;
}


esp_err_t temperatureSensorRead(int16_t *pTemperature)
{
	if (pTemperature == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	if (!s_isDataReady) {
		return ESP_ERR_NOT_FOUND;
	}

	*pTemperature = s_wLastReading;
	return ESP_OK;
}


void temperatureSensorRegisterCallback(TemperatureCallback_t cbCallback)
{
	s_cbTempCallback = cbCallback;
}


esp_err_t temperatureSensorSleep(void)
{
	return s_writeConfig(TEMP_CONFIG_STANDBY);
}


esp_err_t temperatureSensorWake(void)
{
	return s_writeConfig(TEMP_CONFIG_ACTIVE);
}


#ifdef PROJECT_UNIT_TESTS
void temperatureSensorTestSetUp(void)
{
	s_pTaskHandler = NULL;
	s_pSemaphoreHandle = NULL;
	s_cbTempCallback = NULL;
	s_wLastReading = 0;
	s_isDataReady = false;
}
#endif
