#include "door_sensor.h"
#include "device_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <driver/gpio.h>

#define DEBOUNCE_TIME_MS			50
#define TASK_STACK_SIZE				2560
#define TASK_PRIORITY				5
#define TASK_CORE					1

static const char *TAG = "door_sensor";

static TaskHandle_t s_pTaskHandler = NULL;
static SemaphoreHandle_t s_pSemaphoreHandle = NULL;
static StaticSemaphore_t s_sSemaphoreBuffer;
static DoorSensorCallback_t s_cbDoorCallback = NULL;


// ---------------------------------------------------------------------------
// ISR handler
// ---------------------------------------------------------------------------

static void IRAM_ATTR s_doorSensorIntrEvent(void *pArg)
{
	BaseType_t ubHigherPriorityTaskWoken = pdFALSE;

	// Disable interrupt immediately to prevent re-entry during debounce
	gpio_isr_handler_remove(CONFIG_PIN_DOOR_SENSOR);

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
		.pin_bit_mask = (1ULL << CONFIG_PIN_DOOR_SENSOR),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_ANYEDGE,
	};
	gpio_config(&sGpioConfig);
	gpio_isr_handler_add(CONFIG_PIN_DOOR_SENSOR, s_doorSensorIntrEvent, NULL);
}


// ---------------------------------------------------------------------------
// Processing task
// ---------------------------------------------------------------------------

static void s_doorSensorTask(void *pArgs)
{
	#ifdef PROJECT_UNIT_TESTS
		bool isInfiniteLoop = false;
	#else
		bool isInfiniteLoop = true;
	#endif

	do {
		// Block until ISR signals a state change
		xSemaphoreTake(s_pSemaphoreHandle, portMAX_DELAY);
		ESP_LOGI(TAG, "semaphore take");

		// Debounce — wait then re-read the stable level
		vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));

		bool isDoorOpen = doorSensorIsOpen();

		// Notify via callback (if registered)
		if (NULL != s_cbDoorCallback) {
			s_cbDoorCallback(isDoorOpen);
		}

		ESP_LOGI(TAG, "door status: %s", isDoorOpen ? "open" : "closed");

		// Re-enable ISR after processing
		gpio_isr_handler_add(CONFIG_PIN_DOOR_SENSOR, s_doorSensorIntrEvent, NULL);
	} while (isInfiniteLoop);

	s_pTaskHandler = NULL;
	vTaskDelete(NULL);
}


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool doorSensorIsOpen(void)
{
	// Sensor is active-low: GPIO low = door open
	return 0 == gpio_get_level(CONFIG_PIN_DOOR_SENSOR);
}

void doorSensorRegisterCallback(DoorSensorCallback_t cbCallback)
{
	s_cbDoorCallback = cbCallback;
}

esp_err_t doorSensorInit(void)
{
	s_pSemaphoreHandle = xSemaphoreCreateBinaryStatic(&s_sSemaphoreBuffer);

	s_configureGpio();

	// Create processing task on core 1
	if (!xTaskCreatePinnedToCore(s_doorSensorTask, "door_sensor", TASK_STACK_SIZE,
			NULL, TASK_PRIORITY, &s_pTaskHandler, TASK_CORE)) {
		ESP_LOGE(TAG, "Error starting task");
		return ESP_FAIL;
	}

	return ESP_OK;
}

#ifdef PROJECT_UNIT_TESTS
void doorSensorTestSetUp(void)
{
	s_pTaskHandler = NULL;
	s_pSemaphoreHandle = NULL;
	s_cbDoorCallback = NULL;
}
#endif
