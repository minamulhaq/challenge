#include "stdio.h"
#include <unistd.h>
#include "unity.h"
#include <driver/gpio.h>
#include "door_sensor.h"
#include "device_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "Mockgpio.h"
#include "Mockqueue.h"
#include "Mocktask.h"

#define DEBOUNCE_TIME_MS            50

static const SemaphoreHandle_t s_pFakeSemaphore = (SemaphoreHandle_t)0x12345;
static TaskFunction_t s_pTask = NULL;
static gpio_isr_t s_gpioIsr = NULL;
static bool s_isDoorOpen = false;

void setUp(void)
{
    s_pTask = NULL;
    s_gpioIsr = NULL;
    s_isDoorOpen = false;

    doorSensorTestSetUp();
    doorSensorRegisterCallback(NULL);
}

// ---------------------------------------------------------------------------
// Stub callbacks
// ---------------------------------------------------------------------------

static esp_err_t s_callbackGpioIsrHandlerAdd(
    gpio_num_t eGpioNum,
    gpio_isr_t pIsrHandler,
    void *pArgs,
    int lCalls)
{
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
// Helpers
// ---------------------------------------------------------------------------

static void s_initDoorSensorGetTask(void)
{
    gpio_config_ExpectAnyArgsAndReturn(ESP_OK);
    gpio_isr_handler_add_IgnoreAndReturn(ESP_OK);
    xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
    xTaskCreatePinnedToCore_StubWithCallback(s_callbackTask);

    esp_err_t lErr = doorSensorInit();
    TEST_ASSERT_EQUAL_INT(ESP_OK, lErr);
    TEST_ASSERT_NOT_NULL(s_pTask);
}

static void s_doorSensorTaskTest(bool isDoorOpen)
{
    xQueueSemaphoreTake_ExpectAndReturn(s_pFakeSemaphore, portMAX_DELAY, pdTRUE);
    vTaskDelay_Expect(DEBOUNCE_TIME_MS / portTICK_PERIOD_MS);
    gpio_get_level_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, !isDoorOpen);

    gpio_isr_handler_add_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, NULL, NULL, pdTRUE);
    gpio_isr_handler_add_IgnoreArg_isr_handler();
    vTaskDelete_Expect(NULL);

    s_pTask(NULL);
}

static void s_doorSensorCallback(bool isDoorOpen)
{
    s_isDoorOpen = isDoorOpen;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testDoorSensorInitSuccess(void)
{
    gpio_config_t sGpioConfig = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_PIN_DOOR_SENSOR),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config_ExpectAndReturn(&sGpioConfig, ESP_OK);

    gpio_isr_handler_add_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, NULL, NULL, ESP_OK);
    gpio_isr_handler_add_IgnoreArg_isr_handler();

    xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);

    xTaskCreatePinnedToCore_ExpectAndReturn(NULL, "door_sensor", 2560, NULL, 5, NULL, 1, pdTRUE);
    xTaskCreatePinnedToCore_IgnoreArg_pvTaskCode();
    xTaskCreatePinnedToCore_IgnoreArg_pvCreatedTask();

    esp_err_t lErr = doorSensorInit();
    TEST_ASSERT_EQUAL_INT(ESP_OK, lErr);
}

void testDoorSensorInitTaskFail(void)
{
    gpio_config_IgnoreAndReturn(ESP_OK);
    gpio_isr_handler_add_IgnoreAndReturn(ESP_OK);
    xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
    xTaskCreatePinnedToCore_IgnoreAndReturn(pdFALSE);

    esp_err_t lErr = doorSensorInit();
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, lErr);
}

void testDoorSensorIsOpenTrue(void)
{
    // Active-low: level 0 = door open
    gpio_get_level_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, 0);

    bool isOpen = doorSensorIsOpen();
    TEST_ASSERT_TRUE(isOpen);
}

void testDoorSensorIsOpenFalse(void)
{
    // Active-low: level 1 = door closed
    gpio_get_level_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, 1);

    bool isOpen = doorSensorIsOpen();
    TEST_ASSERT_FALSE(isOpen);
}

void testDoorSensorGpioIntrEvent(void)
{
    gpio_config_ExpectAnyArgsAndReturn(ESP_OK);
    gpio_isr_handler_add_StubWithCallback(s_callbackGpioIsrHandlerAdd);
    xQueueGenericCreateStatic_IgnoreAndReturn(s_pFakeSemaphore);
    xTaskCreatePinnedToCore_IgnoreAndReturn(pdTRUE);

    esp_err_t lErr = doorSensorInit();
    TEST_ASSERT_EQUAL_INT(ESP_OK, lErr);

    gpio_isr_handler_remove_ExpectAndReturn(CONFIG_PIN_DOOR_SENSOR, ESP_OK);
    xQueueGiveFromISR_ExpectAndReturn(s_pFakeSemaphore, NULL, pdTRUE);
    xQueueGiveFromISR_IgnoreArg_pxHigherPriorityTaskWoken();

    BaseType_t ulHigherPriorityTaskWoken = pdTRUE;
    xQueueGiveFromISR_ReturnThruPtr_pxHigherPriorityTaskWoken(&ulHigherPriorityTaskWoken);

    s_gpioIsr(NULL);
}

void testDoorSensorTaskOpen(void)
{
    s_initDoorSensorGetTask();
    s_doorSensorTaskTest(true);
}

void testDoorSensorTaskClosed(void)
{
    s_initDoorSensorGetTask();
    s_doorSensorTaskTest(false);
}

void testDoorSensorTaskOpenCallback(void)
{
    doorSensorRegisterCallback(s_doorSensorCallback);
    s_initDoorSensorGetTask();

    s_doorSensorTaskTest(true);
    TEST_ASSERT_TRUE(s_isDoorOpen);
}

void testDoorSensorTaskClosedCallback(void)
{
    doorSensorRegisterCallback(s_doorSensorCallback);
    s_initDoorSensorGetTask();

    s_doorSensorTaskTest(false);
    TEST_ASSERT_FALSE(s_isDoorOpen);
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(testDoorSensorInitSuccess);
    RUN_TEST(testDoorSensorInitTaskFail);

    RUN_TEST(testDoorSensorIsOpenTrue);
    RUN_TEST(testDoorSensorIsOpenFalse);

    RUN_TEST(testDoorSensorGpioIntrEvent);

    RUN_TEST(testDoorSensorTaskOpen);
    RUN_TEST(testDoorSensorTaskClosed);
    RUN_TEST(testDoorSensorTaskOpenCallback);
    RUN_TEST(testDoorSensorTaskClosedCallback);

    int lFailures = UNITY_END();
    return lFailures;
}
