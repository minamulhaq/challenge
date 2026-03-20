# Part 1 — Bug Review: `temperature_sensor`

---

## Bug 1 — ISR handler missing `IRAM_ATTR`

**Category:** FreeRTOS / ESP-IDF platform issue — **Critical**

**Buggy code:**
```c
static void s_tempAlertIntrEvent(void *pArg) {
```

During SPI flash operations (OTA, NVS writes) the CPU stalls its instruction cache. If your ISR is sitting in flash at that moment, the fetch faults with an `IllegalInstruction` or `LoadProhibited` exception and the device crashes. The fix is `IRAM_ATTR`, which tells the linker to put the handler in IRAM where it's always reachable. The reference `door_sensor` already does this correctly.

**Fix:**
```c
static void IRAM_ATTR s_tempAlertIntrEvent(void *pArg)
{
```

---

## Bug 2 — `portYIELD_FROM_ISR()` called unconditionally, return value ignored

**Category:** FreeRTOS / ESP-IDF platform issue

**Buggy code:**
```c
static void s_tempAlertIntrEvent(void *pArg) {
    BaseType_t ulHigherPriorityTaskWoken = pdFALSE;
    gpio_isr_handler_remove(CONFIG_PIN_TEMP_ALERT);
    xSemaphoreGiveFromISR(s_pSemaphoreHandle, &ulHigherPriorityTaskWoken);
    portYIELD_FROM_ISR();
}
```

There are two problems here. First, `portYIELD_FROM_ISR()` with no argument always triggers a context switch, even when no higher-priority task was woken. That's unnecessary CPU churn on every alert interrupt. The correct ESP-IDF form, `portYIELD_FROM_ISR(x)`, only yields when `x` is `pdTRUE`. Second, the return value of `xSemaphoreGiveFromISR` is silently discarded — if the semaphore was already given and the task hasn't consumed it yet, the call returns `pdFALSE` and `ubHigherPriorityTaskWoken` is left in an unreliable state.

**Fix:**
```c
static void IRAM_ATTR s_tempAlertIntrEvent(void *pArg)
{
    BaseType_t ubHigherPriorityTaskWoken = pdFALSE;
    gpio_isr_handler_remove(CONFIG_PIN_TEMP_ALERT);
    BaseType_t ubRet = xSemaphoreGiveFromISR(s_pSemaphoreHandle, &ubHigherPriorityTaskWoken);
    if (pdTRUE == ubRet && pdTRUE == ubHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR(ubHigherPriorityTaskWoken);
    }
}
```

---

## Bug 3 — Wrong variable name prefix in ISR (`ul` instead of `ub` for `BaseType_t`)

**Category:** Coding convention violation

**Buggy code:**
```c
BaseType_t ulHigherPriorityTaskWoken = pdFALSE;
```

The `ul` prefix is reserved for `uint32_t`. `BaseType_t` is a platform-specific signed integer; throughout this codebase it consistently gets the `ub` prefix (see `ubHigherPriorityTaskWoken`, `ubRet` in the reference component).

**Fix:**
```c
BaseType_t ubHigherPriorityTaskWoken = pdFALSE;
```

---

## Bug 4 — `TAG` not declared as `static const char *`

**Category:** Coding convention violation

**Buggy code:**
```c
char TAG[] = "temp_sensor";
```

Two issues: without `static` the symbol has external linkage, which can cause linker collisions if another translation unit also defines `TAG`. Without `const` the string is writable, which wastes RAM and invites accidental modification. The coding rules are explicit: log tag strings must be `static const char *`. The reference component does it right.

**Fix:**
```c
static const char *TAG = "temp_sensor";
```

---

## Bug 5 — ISR function opening brace on same line as declaration

**Category:** Coding convention violation

**Buggy code:**
```c
static void s_tempAlertIntrEvent(void *pArg) {
```

The coding rules are clear: function opening braces always go on a new line.

**Fix:**
```c
static void IRAM_ATTR s_tempAlertIntrEvent(void *pArg)
{
```

---

## Bug 6 — Raw ESP-IDF I2C APIs used instead of `i2cRead()` / `i2cWrite()`

**Category:** Architecture violation — **Critical**

**Buggy code:**
```c
#include <driver/i2c.h>
...
// in s_readTemperature:
esp_err_t lErr = i2c_master_write_read_device(
    I2C_NUM_1, TEMP_SENSOR_I2C_ADDR, (uint8_t[]){TEMP_REG_DATA}, 1,
    data, 2, pdMS_TO_TICKS(100));

// in s_writeConfig:
return i2c_master_write_to_device(
    I2C_NUM_1, TEMP_SENSOR_I2C_ADDR, pTxBuff, 2, pdMS_TO_TICKS(100));
```

The project mandates that all I2C access goes through the shared, mutex-protected `i2cRead()` / `i2cWrite()` wrappers from `hw_i2c.h`. Bypassing them removes the bus mutex, so if two components access I2C simultaneously you'll get bus corruption. It also hard-codes `I2C_NUM_1` and a raw timeout, coupling this component directly to the hardware layout. Replace the include and both call sites.

**Fix:**
```c
#include "hw_i2c.h"
// Remove: #include <driver/i2c.h>

static esp_err_t s_readTemperature(int16_t *pTemperature)
{
    uint8_t pubData[2] = {0};
    esp_err_t lErr = i2cRead(TEMP_SENSOR_I2C_ADDR, TEMP_REG_DATA, pubData, sizeof(pubData));
    if (ESP_OK == lErr) {
        *pTemperature = (int16_t)(((uint16_t)pubData[0] << 8) | pubData[1]);
    }
    return lErr;
}

static esp_err_t s_writeConfig(uint8_t ubValue)
{
    uint8_t pubTxBuff[2] = {TEMP_REG_CONFIG, ubValue};
    return i2cWrite(TEMP_SENSOR_I2C_ADDR, pubTxBuff, sizeof(pubTxBuff));
}
```

---

## Bug 7 — Comparison constant on wrong side in `s_readTemperature`

**Category:** Coding convention violation

**Buggy code:**
```c
if (lErr == ESP_OK) {
```

The coding rules require the constant on the left. This guards against a typo like `lErr = ESP_OK` compiling silently as an assignment; `ESP_OK = lErr` wouldn't compile at all.

**Fix:**
```c
if (ESP_OK == lErr) {
```

---

## Bug 8 — Wrong type and naming for local variable `temperature`

**Category:** Coding convention violation

**Buggy code:**
```c
int temperature = ((int16_t)data[0] << 8) | data[1];
*pTemperature = temperature;
```

`int` is an unspecified-width type with no assigned prefix in the coding rules. The value ends up in an `int16_t`, so use `int16_t` directly with the `w` prefix. Using a wider `int` intermediate can also silently change the value in ways that are hard to spot.

**Fix:**
```c
int16_t wTemperature = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
*pTemperature = wTemperature;
```

---

## Bug 9 — `temperatureSensorInit` definition missing `void` parameter

**Category:** Coding convention violation / C standard issue

**Buggy code:**
```c
esp_err_t temperatureSensorInit() {
```

In C (unlike C++), `f()` means "accepts an unspecified number of arguments" — not "accepts none". The header correctly declares `temperatureSensorInit(void)`, so the definition diverges from it. That's undefined behaviour under the C standard.

**Fix:**
```c
esp_err_t temperatureSensorInit(void)
```

---

## Bug 10 — `temperatureSensorInit` opening brace on same line as declaration

**Category:** Coding convention violation

**Buggy code:**
```c
esp_err_t temperatureSensorInit() {
```

Same rule as Bug 5 — function opening braces must be on a separate line.

**Fix:**
```c
esp_err_t temperatureSensorInit(void)
{
```

---

## Bug 11 — Magic numbers for task stack size, priority, and core

**Category:** Coding convention violation

**Buggy code:**
```c
if (!xTaskCreatePinnedToCore(s_tempSensorTask, "temp_sensor", 1024,
        NULL, 5, &s_pTaskHandler, 1)) {
```

`1024`, `5`, and `1` are meaningless at the call site. Someone reading this code has no idea what those values represent without digging through the FreeRTOS docs. Named macros make the intent obvious and make future changes safe.

**Fix:**
```c
#define TASK_STACK_SIZE     4096
#define TASK_PRIORITY       5
#define TASK_CORE           1

if (!xTaskCreatePinnedToCore(s_tempSensorTask, "temp_sensor", TASK_STACK_SIZE,
        NULL, TASK_PRIORITY, &s_pTaskHandler, TASK_CORE)) {
```

---

## Bug 12 — Task stack size of 1024 bytes is too small

**Category:** FreeRTOS / ESP-IDF platform issue

**Buggy code:**
```c
xTaskCreatePinnedToCore(s_tempSensorTask, "temp_sensor", 1024, ...);
```

The task body uses floating-point arithmetic, `malloc`/`free`, `snprintf`, `ESP_LOG*` (which calls `vprintf` internally), and I2C calls. On ESP32, hardware FP operations and `vprintf` alone can burn several hundred bytes of stack. 1024 bytes will overflow, silently corrupting adjacent memory or triggering a stack-overflow watchdog reset. The reference component uses 2560 bytes for a much simpler task; this one needs at least 4096.

**Fix:**
```c
#define TASK_STACK_SIZE     4096
```

---

## Bug 13 — Comparison constant on wrong side in `temperatureSensorRead`

**Category:** Coding convention violation

**Buggy code:**
```c
if (pTemperature == NULL) {
```

Same rule as Bug 7 — `NULL` (the constant) must be on the left.

**Fix:**
```c
if (NULL == pTemperature) {
```

---

## Bug 14 — Race condition on `s_isDataReady` and `s_wLastReading`

**Category:** Concurrency problem — **Critical**

**Buggy code:**
```c
// Written in sensor task context:
s_wLastReading = wReading;
s_isDataReady = true;

// Read in temperatureSensorRead() from any calling task:
if (!s_isDataReady) {
    return ESP_ERR_NOT_FOUND;
}
*pTemperature = s_wLastReading;
```

These two variables are shared between the sensor task and whatever task calls `temperatureSensorRead()`, with no synchronisation between them. On a dual-core ESP32-S3, both can run simultaneously. A caller can see `s_isDataReady == true`, get preempted before reading `s_wLastReading`, and then read a freshly written — or partially written — value from the sensor task. The C11 memory model gives no atomicity guarantee here.

**Fix:**
```c
// Add at file scope:
static portMUX_TYPE s_sMux = portMUX_INITIALIZER_UNLOCKED;

// In task context — wrap both writes:
portENTER_CRITICAL(&s_sMux);
s_wLastReading = wReading;
s_isDataReady = true;
portEXIT_CRITICAL(&s_sMux);

// In temperatureSensorRead() — wrap both reads:
portENTER_CRITICAL(&s_sMux);
bool isReady = s_isDataReady;
int16_t wCachedReading = s_wLastReading;
portEXIT_CRITICAL(&s_sMux);

if (!isReady) {
    return ESP_ERR_NOT_FOUND;
}
*pTemperature = wCachedReading;
return ESP_OK;
```

---

## Bug 15 — Memory leak: `pAlertMsg` not freed when alert condition is false

**Category:** Resource management issue

**Buggy code:**
```c
char *pAlertMsg = (char *)malloc(48);
if (pAlertMsg != NULL) {
    snprintf(pAlertMsg, 48, "temp_alert:raw=%d,c=%.1f", wReading, result);
}

if (5000 < wReading || -1000 > wReading) {
    ESP_LOGW(TAG, "%s", pAlertMsg);
    free(pAlertMsg);   // only freed inside this branch
}
// leaked when condition is false
```

Every loop iteration allocates 48 bytes, but only frees it when the reading is out of range. In normal operation (readings within range), that allocation leaks every time. Run this long enough and the heap is exhausted — every subsequent `malloc` returns NULL and the firmware starts malfunctioning.

**Fix:**
```c
char *pAlertMsg = (char *)malloc(48);
if (NULL != pAlertMsg) {
    snprintf(pAlertMsg, 48, "temp_alert:raw=%d,c=%.1f", wReading, result);
    if (5000 < wReading || -1000 > wReading) {
        ESP_LOGW(TAG, "%s", pAlertMsg);
    }
    free(pAlertMsg);
}
```

---

## Bug 16 — NULL pointer passed to `ESP_LOGW` when `malloc` fails

**Category:** Resource management / crash risk

**Buggy code:**
```c
char *pAlertMsg = (char *)malloc(48);
if (pAlertMsg != NULL) {
    snprintf(pAlertMsg, 48, ...);
}

if (5000 < wReading || -1000 > wReading) {
    ESP_LOGW(TAG, "%s", pAlertMsg);  // pAlertMsg may be NULL here
    free(pAlertMsg);
}
```

If `malloc` returns NULL and the alert condition is true, `ESP_LOGW` gets a NULL `%s` argument — that's undefined behaviour and almost always a crash. The fix from Bug 15 (keeping the log and free inside the `NULL != pAlertMsg` guard) eliminates this path entirely.

**Fix:** Covered by the fix in Bug 15.

---

## Bug 17 — `malloc` inside recurring task loop causes heap fragmentation

**Category:** Architecture violation / resource management

**Buggy code:**
```c
do {
    ...
    char *pAlertMsg = (char *)malloc(48);
    ...
} while (isInfiniteLoop);
```

Repeated `malloc`/`free` cycles in a FreeRTOS task are an embedded anti-pattern. Even if every allocation is eventually freed, the allocator fragments the heap over time. On a system with limited SRAM, you can end up unable to allocate even when plenty of aggregate free memory exists. Use a stack-local buffer instead — it costs nothing and has no fragmentation risk.

**Fix:**
```c
char pAlertMsg[48];
snprintf(pAlertMsg, sizeof(pAlertMsg), "temp_alert:raw=%d,c=%.1f", wReading, result);
if (5000 < wReading || -1000 > wReading) {
    ESP_LOGW(TAG, "%s", pAlertMsg);
}
```

---

## Bug 18 — GPIO ISR handler not removed on `temperatureSensorInit` failure

**Category:** Resource management issue

**Buggy code:**
```c
esp_err_t temperatureSensorInit(void) {
    s_pSemaphoreHandle = xSemaphoreCreateBinaryStatic(&s_sSemaphoreBuffer);
    s_configureGpio();   // installs GPIO ISR handler

    if (!xTaskCreatePinnedToCore(...)) {
        ESP_LOGE(TAG, "Error starting task");
        return ESP_FAIL;   // GPIO handler is NOT removed
    }
    return ESP_OK;
}
```

`s_configureGpio()` registers the ISR handler before the task is created. If task creation fails, the handler stays registered with no task to service the semaphore. The next retry call will register a second handler on the same pin, causing double-firing interrupts and unpredictable behaviour.

**Fix:**
```c
if (!xTaskCreatePinnedToCore(s_tempSensorTask, "temp_sensor", TASK_STACK_SIZE,
        NULL, TASK_PRIORITY, &s_pTaskHandler, TASK_CORE)) {
    ESP_LOGE(TAG, "Error starting task");
    gpio_isr_handler_remove(CONFIG_PIN_TEMP_ALERT);
    return ESP_FAIL;
}
```

---

## Bug 19 — No guard against double initialisation

**Category:** Resource management issue

**Buggy code:**
```c
esp_err_t temperatureSensorInit(void) {
    s_pSemaphoreHandle = xSemaphoreCreateBinaryStatic(&s_sSemaphoreBuffer);
    s_configureGpio();
    if (!xTaskCreatePinnedToCore(...)) { ... }
    return ESP_OK;
}
```

Calling this a second time while already running overwrites `s_pTaskHandler` without deleting the running task (task leak), registers the GPIO ISR a second time on the same pin (double-fire), and replaces the semaphore handle while the running task may be blocked on the old one (potential deadlock or crash).

**Fix:**
```c
esp_err_t temperatureSensorInit(void)
{
    if (NULL != s_pTaskHandler) {
        ESP_LOGW(TAG, "already initialised");
        return ESP_ERR_INVALID_STATE;
    }
    ...
}
```

---

## Bug 20 — `float result` missing `f` prefix

**Category:** Coding convention violation

**Buggy code:**
```c
float result = (float)wReading / 256.0f;
```

The coding rules assign the `f` prefix to all `float` variables. `result` has no prefix.

**Fix:**
```c
float fResult = (float)wReading / 256.0f;
```

---

## Bug 21 — `uint8_t data[2]` missing type prefix

**Category:** Coding convention violation

**Buggy code:**
```c
uint8_t data[2] = {0};
```

Every variable must carry a type-based prefix. For a `uint8_t` array the prefix is `pub` (`p` for array/pointer + `ub` for `uint8_t`). The variable `data` has no prefix at all.

**Fix:**
```c
uint8_t pubData[2] = {0};
```

---

## Bug 22 — Comparison constants on wrong side in `s_tempSensorTask`

**Category:** Coding convention violation

**Buggy code:**
```c
if (lErr != ESP_OK) {       // line 97
    ...
}
...
if (pAlertMsg != NULL) {    // line 113
    ...
}
```

Same rule as Bugs 7 and 13 — constants (`ESP_OK`, `NULL`) must be on the left side of comparisons. Both instances are in `s_tempSensorTask`.

**Fix:**
```c
if (ESP_OK != lErr) {
    ...
}
...
if (NULL != pAlertMsg) {
    ...
}
```

---

## Summary Table

| # | Category | Severity |
|---|----------|----------|
| 1 | Platform — ISR missing `IRAM_ATTR` | Critical |
| 2 | FreeRTOS — unconditional `portYIELD_FROM_ISR`, return value ignored | High |
| 3 | Convention — wrong prefix `ul` for `BaseType_t` variable | Low |
| 4 | Convention — `TAG` not `static const char *` | Low |
| 5 | Convention — ISR brace style | Low |
| 6 | Architecture — raw `i2c_master_*` calls instead of `i2cRead`/`i2cWrite` | Critical |
| 7 | Convention — comparison constant on wrong side (`lErr == ESP_OK`) | Low |
| 8 | Convention — wrong type/prefix for local `temperature` variable | Low |
| 9 | Convention — `init` definition missing `void` | Low |
| 10 | Convention — `init` brace style | Low |
| 11 | Convention — magic numbers for stack/priority/core | Low |
| 12 | Platform — stack size 1024 bytes too small (float + malloc + snprintf) | High |
| 13 | Convention — comparison constant on wrong side (`pTemperature == NULL`) | Low |
| 14 | Concurrency — unsynchronised access to `s_isDataReady`/`s_wLastReading` | Critical |
| 15 | Resource — `pAlertMsg` memory leak when alert condition false | High |
| 16 | Crash — NULL `pAlertMsg` passed to `ESP_LOGW` on malloc failure | High |
| 17 | Architecture — `malloc` inside recurring task loop (heap fragmentation) | Medium |
| 18 | Resource — GPIO handler not removed on init failure path | Medium |
| 19 | Resource — no guard against double initialisation | Medium |
| 20 | Convention — `float result` missing `f` prefix | Low |
| 21 | Convention — `uint8_t data[2]` missing type prefix | Low |
| 22 | Convention — comparison constants on wrong side in `s_tempSensorTask` | Low |
