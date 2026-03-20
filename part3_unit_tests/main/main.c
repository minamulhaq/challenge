#include "stdio.h"
#include <unistd.h>
#include "unity.h"
#include <driver/gpio.h>
#include "ambient_light.h"
#include "device_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "Mockgpio.h"
#include "Mockqueue.h"
#include "Mocktask.h"
#include "Mockhw_i2c.h"

// Mirror implementation constants so test assertions are self-documenting
#define ALS3000_ADDR		0x29
#define REG_CONTROL			0x00
#define REG_CONFIG			0x01
#define REG_DATA_LOW		0x04
#define REG_THRESHOLD_HIGH	0x06
#define REG_THRESHOLD_LOW	0x07
#define REG_INT_STATUS		0x08

#define DEBOUNCE_TIME_MS	30

static const SemaphoreHandle_t s_pFakeSemaphore = (SemaphoreHandle_t)0x12345;

static TaskFunction_t s_pTask = NULL;
static gpio_isr_t s_gpioIsr = NULL;
static uint16_t s_uwCallbackLux = 0;
static bool s_isCallbackInvoked = false;

// Written bytes captured by s_callbackI2cWriteCapture
static uint8_t s_pubWrittenBuf[4] = {0};
static uint32_t s_ulWrittenSize = 0;

// Lux bytes returned by i2cRead stubs
static uint8_t s_pubI2cReadLuxBuf[2] = {0};

void setUp(void)
{
	s_pTask = NULL;
	s_gpioIsr = NULL;
	s_uwCallbackLux = 0;
	s_isCallbackInvoked = false;
	s_ulWrittenSize = 0;
	s_pubWrittenBuf[0] = s_pubWrittenBuf[1] = s_pubWrittenBuf[2] = s_pubWrittenBuf[3] = 0;
	s_pubI2cReadLuxBuf[0] = s_pubI2cReadLuxBuf[1] = 0;

	ambientLightTestSetUp();
	ambientLightRegisterCallback(NULL);
}


// ---------------------------------------------------------------------------
// Stub callbacks — ISR / task capture
// ---------------------------------------------------------------------------

static esp_err_t s_callbackGpioIsrHandlerAdd(
	gpio_num_t eGpioNum,
	gpio_isr_t pIsrHandler,
	void *pArgs,
	int lCalls)
{
	TEST_ASSERT_EQUAL_INT(CONFIG_PIN_AMBIENT_LIGHT_ALERT, eGpioNum);
	TEST_ASSERT_NOT_NULL(pIsrHandler);
	s_gpioIsr = pIsrHandler;
	return ESP_OK;
}

static BaseType_t s_callbackTask(
	TaskFunction_t pTaskCode,
	const char *const szName,
	const uint32_t ulStackDepth,
	void *const pParameters,
	UBaseType_t ulPriority,
	TaskHandle_t *const pCreatedTask,
	const BaseType_t lCoreID,
	int lCmockNumCalls)
{
	TEST_ASSERT_NOT_NULL(pTaskCode);
	s_pTask = pTaskCode;
	return pdTRUE;
}


// ---------------------------------------------------------------------------
// Stub callbacks — I2C write verification
// ---------------------------------------------------------------------------

// Verifies the exact 4-call I2C write sequence in ambientLightInit
static esp_err_t s_callbackI2cWriteInit(
	uint8_t ubAddr,
	uint8_t *pVal,
	uint32_t ulSize,
	int lCalls)
{
	TEST_ASSERT_EQUAL_UINT8(ALS3000_ADDR, ubAddr);

	switch (lCalls) {
	case 0: // CONTROL = 0x01 (active mode)
		TEST_ASSERT_EQUAL_UINT32(2, ulSize);
		TEST_ASSERT_EQUAL_UINT8(REG_CONTROL, pVal[0]);
		TEST_ASSERT_EQUAL_UINT8(0x01, pVal[1]);
		break;
	case 1: // CONFIG bit0 = 1 (interrupt enable)
		TEST_ASSERT_EQUAL_UINT32(2, ulSize);
		TEST_ASSERT_EQUAL_UINT8(REG_CONFIG, pVal[0]);
		TEST_ASSERT_EQUAL_UINT8(0x01, pVal[1]);
		break;
	case 2: // THRESHOLD_HIGH = 1000 lux (0x03E8 little-endian)
		TEST_ASSERT_EQUAL_UINT32(3, ulSize);
		TEST_ASSERT_EQUAL_UINT8(REG_THRESHOLD_HIGH, pVal[0]);
		TEST_ASSERT_EQUAL_UINT8(0xE8, pVal[1]); // low byte  of 1000
		TEST_ASSERT_EQUAL_UINT8(0x03, pVal[2]); // high byte of 1000
		break;
	case 3: // THRESHOLD_LOW = 0x0000
		TEST_ASSERT_EQUAL_UINT32(3, ulSize);
		TEST_ASSERT_EQUAL_UINT8(REG_THRESHOLD_LOW, pVal[0]);
		TEST_ASSERT_EQUAL_UINT8(0x00, pVal[1]);
		TEST_ASSERT_EQUAL_UINT8(0x00, pVal[2]);
		break;
	default:
		TEST_FAIL_MESSAGE("Unexpected extra i2cWrite call during init");
		break;
	}

	return ESP_OK;
}

// Captures the first write buffer for post-call byte-level assertions
static esp_err_t s_callbackI2cWriteCapture(
	uint8_t ubAddr,
	uint8_t *pVal,
	uint32_t ulSize,
	int lCalls)
{
	TEST_ASSERT_EQUAL_UINT8(ALS3000_ADDR, ubAddr);
	s_ulWrittenSize = ulSize;

	uint32_t ulCopy = ulSize < sizeof(s_pubWrittenBuf) ? ulSize : sizeof(s_pubWrittenBuf);
	for (uint32_t i = 0; i < ulCopy; i++) {
		s_pubWrittenBuf[i] = pVal[i];
	}

	return ESP_OK;
}


// ---------------------------------------------------------------------------
// Stub callbacks — I2C read
// ---------------------------------------------------------------------------

// Returns lux data from s_pubI2cReadLuxBuf for ambientLightRead tests
static esp_err_t s_callbackI2cReadLux(
	uint8_t ubAddr,
	uint8_t ubReg,
	uint8_t *pVal,
	uint32_t ulSize,
	int lCalls)
{
	TEST_ASSERT_EQUAL_UINT8(ALS3000_ADDR, ubAddr);
	TEST_ASSERT_EQUAL_UINT8(REG_DATA_LOW, ubReg);
	TEST_ASSERT_EQUAL_UINT32(2, ulSize);
	pVal[0] = s_pubI2cReadLuxBuf[0];
	pVal[1] = s_pubI2cReadLuxBuf[1];
	return ESP_OK;
}

// Handles both INT_STATUS (clear) and DATA_LOW (lux) reads inside the task
static esp_err_t s_callbackI2cReadTask(
	uint8_t ubAddr,
	uint8_t ubReg,
	uint8_t *pVal,
	uint32_t ulSize,
	int lCalls)
{
	TEST_ASSERT_EQUAL_UINT8(ALS3000_ADDR, ubAddr);

	if (REG_INT_STATUS == ubReg) {
		TEST_ASSERT_EQUAL_UINT32(1, ulSize);
		pVal[0] = 0x01; // threshold-exceeded flag
	} else if (REG_DATA_LOW == ubReg) {
		TEST_ASSERT_EQUAL_UINT32(2, ulSize);
		pVal[0] = s_pubI2cReadLuxBuf[0];
		pVal[1] = s_pubI2cReadLuxBuf[1];
	} else {
		TEST_FAIL_MESSAGE("Unexpected i2cRead register in task");
	}

	return ESP_OK;
}


// ---------------------------------------------------------------------------
// User callback
// ---------------------------------------------------------------------------

static void s_ambientLightCallback(uint16_t uwLux)
{
	s_isCallbackInvoked = true;
	s_uwCallbackLux = uwLux;
}


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void s_initAmbientLight(void)
{
	gpio_config_IgnoreAndReturn(ESP_OK);
	gpio_isr_handler_add_StubWithCallback(s_callbackGpioIsrHandlerAdd);
	xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
	i2cWrite_IgnoreAndReturn(ESP_OK);
	i2cRead_IgnoreAndReturn(ESP_OK);
	xTaskCreatePinnedToCore_StubWithCallback(s_callbackTask);

	esp_err_t eRet = ambientLightInit();
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
	TEST_ASSERT_NOT_NULL(s_pTask);
	TEST_ASSERT_NOT_NULL(s_gpioIsr);
}

static void s_triggerIsr(void)
{
	gpio_isr_handler_remove_ExpectAndReturn(CONFIG_PIN_AMBIENT_LIGHT_ALERT, ESP_OK);
	xQueueGiveFromISR_ExpectAndReturn(s_pFakeSemaphore, NULL, pdTRUE);
	xQueueGiveFromISR_IgnoreArg_pxHigherPriorityTaskWoken();
	BaseType_t ubHigherPriorityTaskWoken = pdTRUE;
	xQueueGiveFromISR_ReturnThruPtr_pxHigherPriorityTaskWoken(&ubHigherPriorityTaskWoken);

	s_gpioIsr(NULL);
}

// Runs one pass of the task (PROJECT_UNIT_TESTS → exits after one iteration).
// Caller must set i2cRead_StubWithCallback before calling.
static void s_runAlertTask(void)
{
	xQueueSemaphoreTake_ExpectAndReturn(s_pFakeSemaphore, portMAX_DELAY, pdTRUE);
	vTaskDelay_Expect(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
	gpio_isr_handler_add_IgnoreAndReturn(ESP_OK); // re-enable at end of task
	vTaskDelete_Expect(NULL);

	s_pTask(NULL);
}


// ---------------------------------------------------------------------------
// Tests: Init
// ---------------------------------------------------------------------------

void testAmbientLightInitSuccess(void)
{
	gpio_config_t sGpioConfig = {
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = (1ULL << CONFIG_PIN_AMBIENT_LIGHT_ALERT),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_NEGEDGE,
	};
	gpio_config_ExpectAndReturn(&sGpioConfig, ESP_OK);

	gpio_isr_handler_add_ExpectAndReturn(CONFIG_PIN_AMBIENT_LIGHT_ALERT, NULL, NULL, ESP_OK);
	gpio_isr_handler_add_IgnoreArg_isr_handler();

	xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);

	// Verifies exact register address + value bytes for all 4 writes
	i2cWrite_StubWithCallback(s_callbackI2cWriteInit);

	// Verify INT_STATUS is read to clear any pending interrupt
	i2cRead_ExpectAndReturn(ALS3000_ADDR, REG_INT_STATUS, NULL, 1, ESP_OK);
	i2cRead_IgnoreArg_pVal();

	xTaskCreatePinnedToCore_ExpectAndReturn(NULL, "ambient_light", 2560, NULL, 5, NULL, 1, pdTRUE);
	xTaskCreatePinnedToCore_IgnoreArg_pvTaskCode();
	xTaskCreatePinnedToCore_IgnoreArg_pvCreatedTask();

	esp_err_t eRet = ambientLightInit();
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
}

void testAmbientLightInitTaskFail(void)
{
	gpio_config_IgnoreAndReturn(ESP_OK);
	gpio_isr_handler_add_IgnoreAndReturn(ESP_OK);
	xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
	i2cWrite_IgnoreAndReturn(ESP_OK);
	i2cRead_IgnoreAndReturn(ESP_OK);
	xTaskCreatePinnedToCore_IgnoreAndReturn(pdFALSE);
	gpio_isr_handler_remove_ExpectAndReturn(CONFIG_PIN_AMBIENT_LIGHT_ALERT, ESP_OK);

	esp_err_t eRet = ambientLightInit();
	TEST_ASSERT_EQUAL_INT(ESP_FAIL, eRet);
}

void testAmbientLightInitI2cFail(void)
{
	// Any I2C failure → must remove the GPIO ISR handler as cleanup
	gpio_config_IgnoreAndReturn(ESP_OK);
	gpio_isr_handler_add_IgnoreAndReturn(ESP_OK);
	xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
	i2cWrite_IgnoreAndReturn(ESP_FAIL);
	gpio_isr_handler_remove_ExpectAndReturn(CONFIG_PIN_AMBIENT_LIGHT_ALERT, ESP_OK);

	esp_err_t eRet = ambientLightInit();
	TEST_ASSERT_EQUAL_INT(ESP_FAIL, eRet);
}


// ---------------------------------------------------------------------------
// Tests: Read
// ---------------------------------------------------------------------------

void testAmbientLightReadNullPtr(void)
{
	esp_err_t eRet = ambientLightRead(NULL);
	TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, eRet);
}

void testAmbientLightReadSuccess(void)
{
	// 1000 lux = 0x03E8 → DATA_LOW=0xE8, DATA_HIGH=0x03
	s_pubI2cReadLuxBuf[0] = 0xE8;
	s_pubI2cReadLuxBuf[1] = 0x03;

	i2cRead_StubWithCallback(s_callbackI2cReadLux);

	uint16_t uwLux = 0;
	esp_err_t eRet = ambientLightRead(&uwLux);
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
	TEST_ASSERT_EQUAL_UINT16(1000, uwLux);
}

void testAmbientLightReadI2cFail(void)
{
	i2cRead_IgnoreAndReturn(ESP_FAIL);

	uint16_t uwLux = 0;
	esp_err_t eRet = ambientLightRead(&uwLux);
	TEST_ASSERT_EQUAL_INT(ESP_FAIL, eRet);
}


// ---------------------------------------------------------------------------
// Tests: Set threshold
// ---------------------------------------------------------------------------

void testAmbientLightSetThreshold(void)
{
	// 0x1234 → little-endian: low=0x34, high=0x12
	i2cWrite_StubWithCallback(s_callbackI2cWriteCapture);

	esp_err_t eRet = ambientLightSetThreshold(0x1234);
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
	TEST_ASSERT_EQUAL_UINT32(3, s_ulWrittenSize);
	TEST_ASSERT_EQUAL_UINT8(REG_THRESHOLD_HIGH, s_pubWrittenBuf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x34, s_pubWrittenBuf[1]); // low byte
	TEST_ASSERT_EQUAL_UINT8(0x12, s_pubWrittenBuf[2]); // high byte
}


// ---------------------------------------------------------------------------
// Tests: Sleep / Wake
// ---------------------------------------------------------------------------

void testAmbientLightSleepWritesCorrectBytes(void)
{
	i2cWrite_StubWithCallback(s_callbackI2cWriteCapture);

	esp_err_t eRet = ambientLightSleep();
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
	TEST_ASSERT_EQUAL_UINT32(2, s_ulWrittenSize);
	TEST_ASSERT_EQUAL_UINT8(REG_CONTROL, s_pubWrittenBuf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x00, s_pubWrittenBuf[1]); // standby
}

void testAmbientLightWakeWritesCorrectBytes(void)
{
	i2cWrite_StubWithCallback(s_callbackI2cWriteCapture);

	esp_err_t eRet = ambientLightWake();
	TEST_ASSERT_EQUAL_INT(ESP_OK, eRet);
	TEST_ASSERT_EQUAL_UINT32(2, s_ulWrittenSize);
	TEST_ASSERT_EQUAL_UINT8(REG_CONTROL, s_pubWrittenBuf[0]);
	TEST_ASSERT_EQUAL_UINT8(0x01, s_pubWrittenBuf[1]); // active
}


// ---------------------------------------------------------------------------
// Tests: Alert flow (ISR → semaphore → task)
// ---------------------------------------------------------------------------

void testAmbientLightGpioIntrEvent(void)
{
	s_initAmbientLight();

	// ISR must remove its own handler first (prevents re-entry), then give semaphore
	gpio_isr_handler_remove_ExpectAndReturn(CONFIG_PIN_AMBIENT_LIGHT_ALERT, ESP_OK);
	xQueueGiveFromISR_ExpectAndReturn(s_pFakeSemaphore, NULL, pdTRUE);
	xQueueGiveFromISR_IgnoreArg_pxHigherPriorityTaskWoken();
	BaseType_t ubHigherPriorityTaskWoken = pdTRUE;
	xQueueGiveFromISR_ReturnThruPtr_pxHigherPriorityTaskWoken(&ubHigherPriorityTaskWoken);

	s_gpioIsr(NULL);
}

void testAmbientLightAlertFlow(void)
{
	// 256 lux: DATA_LOW=0x00, DATA_HIGH=0x01
	s_pubI2cReadLuxBuf[0] = 0x00;
	s_pubI2cReadLuxBuf[1] = 0x01;

	s_initAmbientLight();
	s_triggerIsr();

	// Task: debounce → read INT_STATUS (clears ALERT) → read lux → re-enable ISR
	i2cRead_StubWithCallback(s_callbackI2cReadTask);
	s_runAlertTask();
}

void testAmbientLightAlertFlowWithCallback(void)
{
	// 1000 lux = 0x03E8
	s_pubI2cReadLuxBuf[0] = 0xE8;
	s_pubI2cReadLuxBuf[1] = 0x03;

	ambientLightRegisterCallback(s_ambientLightCallback);
	s_initAmbientLight();
	s_triggerIsr();

	i2cRead_StubWithCallback(s_callbackI2cReadTask);
	s_runAlertTask();

	TEST_ASSERT_TRUE(s_isCallbackInvoked);
	TEST_ASSERT_EQUAL_UINT16(1000, s_uwCallbackLux);
}

void testAmbientLightAlertFlowNoCallback(void)
{
	s_pubI2cReadLuxBuf[0] = 0x64;
	s_pubI2cReadLuxBuf[1] = 0x00; // 100 lux

	// No callback registered — task must complete without crashing
	s_initAmbientLight();
	s_triggerIsr();

	i2cRead_StubWithCallback(s_callbackI2cReadTask);
	s_runAlertTask();

	TEST_ASSERT_FALSE(s_isCallbackInvoked);
}


// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
	UNITY_BEGIN();

	RUN_TEST(testAmbientLightInitSuccess);
	RUN_TEST(testAmbientLightInitTaskFail);
	RUN_TEST(testAmbientLightInitI2cFail);

	RUN_TEST(testAmbientLightReadNullPtr);
	RUN_TEST(testAmbientLightReadSuccess);
	RUN_TEST(testAmbientLightReadI2cFail);

	RUN_TEST(testAmbientLightSetThreshold);

	RUN_TEST(testAmbientLightSleepWritesCorrectBytes);
	RUN_TEST(testAmbientLightWakeWritesCorrectBytes);

	RUN_TEST(testAmbientLightGpioIntrEvent);
	RUN_TEST(testAmbientLightAlertFlow);
	RUN_TEST(testAmbientLightAlertFlowWithCallback);
	RUN_TEST(testAmbientLightAlertFlowNoCallback);

	int lFailures = UNITY_END();
	return lFailures;
}
