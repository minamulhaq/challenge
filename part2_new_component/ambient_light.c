#include "ambient_light.h"
#include "device_config.h"
#include "hw_i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <driver/gpio.h>

#define ALS3000_ADDR			0x29

#define REG_CONTROL				0x00
#define REG_CONFIG				0x01
#define REG_DATA_LOW			0x04
#define REG_DATA_HIGH			0x05
#define REG_THRESHOLD_HIGH		0x06
#define REG_THRESHOLD_LOW		0x07
#define REG_INT_STATUS			0x08

#define DEBOUNCE_TIME_MS		30
#define TASK_STACK_SIZE			2560
#define TASK_PRIORITY			5
#define TASK_CORE				1

#define DEFAULT_THRESHOLD_LUX	1000

static const char *TAG = "ambient_light";

static TaskHandle_t s_pTaskHandler = NULL;
static SemaphoreHandle_t s_pSemaphoreHandle = NULL;
static StaticSemaphore_t s_sSemaphoreBuffer;
static AmbientLightCallback_t s_cbAmbientLightCallback = NULL;


// ---------------------------------------------------------------------------
// ISR handler
// ---------------------------------------------------------------------------

static void IRAM_ATTR s_ambientLightIntrEvent(void *pArg)
{
	BaseType_t ubHigherPriorityTaskWoken = pdFALSE;

	// Disable interrupt immediately to prevent re-entry during debounce
	gpio_isr_handler_remove(CONFIG_PIN_AMBIENT_LIGHT_ALERT);

	BaseType_t ubRet = xSemaphoreGiveFromISR(s_pSemaphoreHandle, &ubHigherPriorityTaskWoken);
	if (pdTRUE == ubRet && pdTRUE == ubHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}


// ---------------------------------------------------------------------------
// GPIO configuration
// ---------------------------------------------------------------------------

static void s_configureGpio(void)
{
	gpio_config_t sGpioConfig = {
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = (1ULL << CONFIG_PIN_AMBIENT_LIGHT_ALERT),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_NEGEDGE,
	};
	gpio_config(&sGpioConfig);
	gpio_isr_handler_add(CONFIG_PIN_AMBIENT_LIGHT_ALERT, s_ambientLightIntrEvent, NULL);
}


// ---------------------------------------------------------------------------
// Processing task
// ---------------------------------------------------------------------------

static void s_ambientLightTask(void *pArgs)
{
	#ifdef PROJECT_UNIT_TESTS
		bool isInfiniteLoop = false;
	#else
		bool isInfiniteLoop = true;
	#endif

	do {
		// Block until ISR signals an alert
		xSemaphoreTake(s_pSemaphoreHandle, portMAX_DELAY);
		ESP_LOGI(TAG, "semaphore take");

		// Debounce
		vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));

		// Read INT_STATUS to clear the ALERT pin
		uint8_t ubIntStatus = 0;
		esp_err_t eRet = i2cRead(ALS3000_ADDR, REG_INT_STATUS, &ubIntStatus, 1);
		if (ESP_OK != eRet) {
			ESP_LOGE(TAG, "Error reading INT_STATUS: %d", eRet);
		}

		// Read lux value (DATA_LOW + DATA_HIGH in one sequential read)
		uint8_t pubData[2] = {0};
		eRet = i2cRead(ALS3000_ADDR, REG_DATA_LOW, pubData, sizeof(pubData));
		if (ESP_OK != eRet) {
			ESP_LOGE(TAG, "Error reading lux data: %d", eRet);
		}

		uint16_t uwLux = ((uint16_t)pubData[1] << 8) | pubData[0];

		ESP_LOGI(TAG, "alert: lux = %u", (unsigned)uwLux);

		if (NULL != s_cbAmbientLightCallback) {
			s_cbAmbientLightCallback(uwLux);
		}

		// Re-enable ISR after processing
		gpio_isr_handler_add(CONFIG_PIN_AMBIENT_LIGHT_ALERT, s_ambientLightIntrEvent, NULL);
	} while (isInfiniteLoop);

	s_pTaskHandler = NULL;
	vTaskDelete(NULL);
}


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

esp_err_t ambientLightRead(uint16_t *pLux)
{
	if (NULL == pLux) {
		return ESP_ERR_INVALID_ARG;
	}

	uint8_t pubData[2] = {0};
	esp_err_t eRet = i2cRead(ALS3000_ADDR, REG_DATA_LOW, pubData, sizeof(pubData));
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error reading lux: %d", eRet);
		return eRet;
	}

	*pLux = ((uint16_t)pubData[1] << 8) | pubData[0];
	return ESP_OK;
}

esp_err_t ambientLightSetThreshold(uint16_t uwThreshold)
{
	// Write 3 bytes: [reg_addr, low_byte, high_byte] (little-endian threshold)
	uint8_t pubBuf[3] = {
		REG_THRESHOLD_HIGH,
		(uint8_t)(uwThreshold & 0xFF),
		(uint8_t)((uwThreshold >> 8) & 0xFF),
	};
	return i2cWrite(ALS3000_ADDR, pubBuf, sizeof(pubBuf));
}

esp_err_t ambientLightSleep(void)
{
	uint8_t pubBuf[2] = { REG_CONTROL, 0x00 }; // standby
	return i2cWrite(ALS3000_ADDR, pubBuf, sizeof(pubBuf));
}

esp_err_t ambientLightWake(void)
{
	uint8_t pubBuf[2] = { REG_CONTROL, 0x01 }; // active
	return i2cWrite(ALS3000_ADDR, pubBuf, sizeof(pubBuf));
}

void ambientLightRegisterCallback(AmbientLightCallback_t cbCallback)
{
	s_cbAmbientLightCallback = cbCallback;
}

esp_err_t ambientLightInit(void)
{
	uint8_t pubControl[2]   = { REG_CONTROL, 0x01 };       // active mode
	uint8_t pubConfig[2]    = { REG_CONFIG,  0x01 };       // interrupt enable
	uint8_t pubThreshLow[3] = { REG_THRESHOLD_LOW, 0x00, 0x00 };
	uint8_t ubIntStatus     = 0;
	esp_err_t eRet          = ESP_OK;

	s_pSemaphoreHandle = xSemaphoreCreateBinaryStatic(&s_sSemaphoreBuffer);

	s_configureGpio();

	// Set sensor to active mode
	eRet = i2cWrite(ALS3000_ADDR, pubControl, sizeof(pubControl));
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error setting active mode: %d", eRet);
		goto init_fail;
	}

	// Enable interrupt
	eRet = i2cWrite(ALS3000_ADDR, pubConfig, sizeof(pubConfig));
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error enabling interrupt: %d", eRet);
		goto init_fail;
	}

	// Set default upper threshold to 1000 lux
	eRet = ambientLightSetThreshold(DEFAULT_THRESHOLD_LUX);
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error setting upper threshold: %d", eRet);
		goto init_fail;
	}

	// Set lower threshold to 0x0000 (reserved, per spec)
	eRet = i2cWrite(ALS3000_ADDR, pubThreshLow, sizeof(pubThreshLow));
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error setting lower threshold: %d", eRet);
		goto init_fail;
	}

	// Clear any pending interrupt by reading INT_STATUS
	eRet = i2cRead(ALS3000_ADDR, REG_INT_STATUS, &ubIntStatus, 1);
	if (ESP_OK != eRet) {
		ESP_LOGE(TAG, "Error clearing pending interrupt: %d", eRet);
		goto init_fail;
	}

	// Create processing task pinned to core 1
	if (!xTaskCreatePinnedToCore(s_ambientLightTask, "ambient_light", TASK_STACK_SIZE,
			NULL, TASK_PRIORITY, &s_pTaskHandler, TASK_CORE)) {
		ESP_LOGE(TAG, "Error starting task");
		eRet = ESP_FAIL;
		goto init_fail;
	}

	return ESP_OK;

init_fail:
	gpio_isr_handler_remove(CONFIG_PIN_AMBIENT_LIGHT_ALERT);
	s_pSemaphoreHandle = NULL;
	return ESP_FAIL;
}

#ifdef PROJECT_UNIT_TESTS
void ambientLightTestSetUp(void)
{
	s_pTaskHandler = NULL;
	s_pSemaphoreHandle = NULL;
	s_cbAmbientLightCallback = NULL;
}
#endif
