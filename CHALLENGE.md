# Embedded Software Engineer — Coding Challenge

Read this document fully before starting. Skim the reference component and coding rules first.

---

## Context

You are joining a team that develops ESP-IDF firmware for an ESP32-S3 based medical device.

The firmware is component-based: each hardware driver lives in its own directory with a
public header (`.h`), implementation (`.c`), and CMake build file (`CMakeLists.txt`).

Key architectural patterns:

- **ISR → semaphore → task**: GPIO interrupts signal a binary semaphore; a dedicated
  FreeRTOS task blocks on it, performs debounce/processing, then completes the operation.
- **I2C abstraction**: All I2C devices use a shared, mutex-protected bus via `i2cRead()` /
  `i2cWrite()` from `hw_i2c.h`. Never call raw ESP-IDF I2C APIs directly.
- **Logging**: Use `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()` from the standard ESP-IDF
  `esp_log.h`.
- **Unit tests**: Host-based (Linux target), using Unity assertions + CMock for mocking
  all ESP-IDF and component dependencies.

Study `reference/door_sensor.*` — it is a complete, convention-compliant component that
demonstrates all these patterns.

---

## Part 1 — Bug Review

### Files: `part1_bug_review/temperature_sensor.*`

A colleague wrote a temperature sensor component for an I2C temperature sensor. The code
compiles but contains **multiple bugs** spanning:

- **Coding convention violations** (naming, braces, comparisons, etc.)
- **FreeRTOS / ESP-IDF platform issues** (ISR safety, task configuration)
- **Concurrency problems** (race conditions, missing synchronization)
- **Resource management issues** (memory leaks, missing cleanup)
- **Architecture violations** (not following project patterns)

### Your task

Create `part1_bug_review/REVIEW.md` containing:

1. A numbered list of **every bug** you find.
2. For each bug:
   - **What** is wrong (quote the problematic code)
   - **Why** it is a bug (explain the consequence — crash, race condition, convention
     violation, resource leak, etc.)
   - **Fix** — the corrected code snippet

Tip: There are bugs at multiple difficulty levels. Finding all convention violations is
straightforward. The concurrency and platform bugs require understanding how FreeRTOS
and ESP32 hardware work.

---

## Part 2 — New Component

### Specification: `part2_new_component/SPEC.md`

Implement a driver for the fictional **ALS3000 ambient light sensor**. The specification
provides the I2C register map, required API surface, and behavioral requirements.

### Your deliverables

Place these files in `part2_new_component/`:

- `ambient_light.h` — Public API header with documentation
- `ambient_light.c` — Full implementation
- `CMakeLists.txt` — Component build file

### Requirements

1. Follow **all** conventions from `coding_rules/README_C_code_rules.md`.
2. Use the same architectural patterns as the reference `door_sensor` component:
   - ISR → binary semaphore → task for the ALERT interrupt
   - `i2cRead()` / `i2cWrite()` for all bus operations
   - `ESP_LOG*()` for logging
3. Implement all API functions listed in the spec.
4. Handle errors properly — check return values, clean up on init failure.
5. Include `#ifdef PROJECT_UNIT_TESTS` hooks for unit test support.
6. Your code must build and tests must pass (see `BUILD.md`).

---

## Part 3 — Unit Tests

### Guide: `part3_unit_tests/README.md`

Write unit tests for your Part 2 ambient light sensor component.

### Your deliverables

Create the following structure in `part3_unit_tests/`:

```
part3_unit_tests/
├── CMakeLists.txt
├── sdkconfig.defaults
└── main/
    ├── CMakeLists.txt
    └── main.c
```

### Requirements

1. Follow the same CMake structure as `reference/unit_tests/`.
2. Use Unity + CMock (mock all ESP-IDF APIs and project component dependencies).
3. Test at minimum:
   - Successful initialization
   - Initialization failure (task creation fails)
   - Sensor read (I2C success and failure paths)
   - Threshold alert (ISR → task → interrupt clear + sensor read flow)
   - Callback invocation
4. Use `_ExpectAndReturn`, `_IgnoreAndReturn`, and `_StubWithCallback` patterns
   from CMock as demonstrated in the reference tests.

---

## Available APIs (for reference)

You may assume these headers exist and provide the following declarations:

### `hw_i2c.h`

``` c
esp_err_t i2cInit(void);
esp_err_t i2cWrite(uint8_t ubAddr, uint8_t *pVal, uint32_t ulSize);
esp_err_t i2cRead(uint8_t ubAddr, uint8_t ubReg, uint8_t *pVal, uint32_t ulSize);
```

### `esp_log.h` (standard ESP-IDF)

``` c
ESP_LOGI(TAG, fmt, ...)   // Info level
ESP_LOGW(TAG, fmt, ...)   // Warning level
ESP_LOGE(TAG, fmt, ...)   // Error level
```

### `device_config.h`

``` c
// GPIO pin definitions as CONFIG_PIN_* macros
// e.g. CONFIG_PIN_AMBIENT_LIGHT_ALERT
```
