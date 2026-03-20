# ALS3000 Ambient Light Sensor — Component Specification

## Overview

The **ALS3000** is a fictional I2C ambient light sensor with a configurable
threshold-based ALERT interrupt. Your task is to implement a complete driver
component following the project's architectural patterns and coding conventions.

---

## Hardware Interface

| Parameter | Value |
| --- | --- |
| Bus | I2C |
| Address | `0x29` (7-bit) |
| ALERT pin | Active-low, open-drain output. Goes low when reading exceeds upper threshold. Cleared by reading the `INT_STATUS` register. |
| Operating voltage | 3.3V |

The ALERT pin is connected to GPIO `CONFIG_PIN_AMBIENT_LIGHT_ALERT` (defined in
`device_config.h`). An external pull-up resistor is present on the board.

---

## I2C Register Map

| Register | Address | R/W | Size | Description |
| --- | --- | --- | --- | --- |
| `CONTROL` | `0x00` | R/W | 1 byte | Power mode: `0x00` = standby, `0x01` = active (continuous), `0x03` = single-shot |
| `CONFIG` | `0x01` | R/W | 1 byte | Configuration: bit 0 = interrupt enable (`1` = enabled), bits 2:1 = gain (`00` = 1x, `01` = 2x, `10` = 4x) |
| `DATA_LOW` | `0x04` | R | 1 byte | Lower byte of ambient light reading (lux) |
| `DATA_HIGH` | `0x05` | R | 1 byte | Upper byte of ambient light reading (lux) |
| `THRESHOLD_HIGH` | `0x06` | R/W | 2 bytes | Upper threshold (little-endian uint16). Alert fires when reading > threshold. |
| `THRESHOLD_LOW` | `0x07` | R/W | 2 bytes | Lower threshold (little-endian uint16). Reserved for future use — write `0x0000` during init. |
| `INT_STATUS` | `0x08` | R | 1 byte | Interrupt status: bit 0 = threshold exceeded. Reading this register clears the ALERT pin. |

### Reading sensor data

To obtain a lux reading:

1. Read `DATA_LOW` (1 byte) and `DATA_HIGH` (1 byte) sequentially, or read 2 bytes
   starting from `DATA_LOW`.
2. Combine: `uwLux = ((uint16_t)ubDataHigh << 8) | ubDataLow`

### Writing a 2-byte threshold

Write 2 bytes to the threshold register in little-endian order:
`[low_byte, high_byte]`

---

## Required API

Implement the following public API in `ambient_light.h`:

### Types

```c
typedef void (*AmbientLightCallback_t)(uint16_t uwLux);
```

### Functions

| Function | Signature | Description |
| --- | --- | --- |
| `ambientLightInit` | `esp_err_t ambientLightInit(void)` | Initialize: configure GPIO interrupt, create semaphore, create processing task, set sensor to active mode with interrupt enabled, set default threshold to `1000` lux, clear any pending interrupt |
| `ambientLightRead` | `esp_err_t ambientLightRead(uint16_t *pLux)` | Read current ambient light level in lux. Returns `ESP_ERR_INVALID_ARG` if `pLux` is NULL. |
| `ambientLightSetThreshold` | `esp_err_t ambientLightSetThreshold(uint16_t uwThreshold)` | Write upper threshold register. |
| `ambientLightSleep` | `esp_err_t ambientLightSleep(void)` | Put sensor in standby mode (write `0x00` to CONTROL) |
| `ambientLightWake` | `esp_err_t ambientLightWake(void)` | Put sensor in active mode (write `0x01` to CONTROL) |
| `ambientLightRegisterCallback` | `void ambientLightRegisterCallback(AmbientLightCallback_t cbCallback)` | Register/unregister a callback for threshold alerts |

---

## Behavioral Requirements

### Initialization sequence (`ambientLightInit`)

1. Create a static binary semaphore
2. Configure GPIO for ALERT pin (input, pull-up enabled, negative edge interrupt)
3. Set sensor to active mode (write `0x01` to `CONTROL`)
4. Enable interrupt (write `0x01` to `CONFIG`)
5. Set default upper threshold to `1000` lux
6. Set lower threshold to `0x0000`
7. Clear any pending interrupt by reading `INT_STATUS`
8. Create a FreeRTOS task pinned to core 1 (stack: 2560, priority: 5)
9. Return `ESP_FAIL` if any critical step fails; clean up partially initialized resources

### Alert processing (task)

When the ALERT pin fires:

1. ISR removes GPIO handler, gives semaphore (standard ISR → semaphore → task pattern)
2. Task wakes, debounces (30ms delay)
3. Task reads `INT_STATUS` register to clear the interrupt
4. Task reads the ambient light value (`DATA_LOW` + `DATA_HIGH`)
5. If callback is registered, invoke it with the lux value
6. Re-enable GPIO ISR handler

### Power management

- `ambientLightSleep()` writes `0x00` to `CONTROL` (standby mode)
- `ambientLightWake()` writes `0x01` to `CONTROL` (active mode)
- Both must use the project's I2C abstraction (`i2cWrite`)

### Error handling

- All I2C operations must check return values
- `ambientLightRead()` must validate input parameters
- Init must return appropriate error code on failure

---

## Integration

- **I2C**: Use `i2cRead()` and `i2cWrite()` from `hw_i2c.h`. Never use raw ESP-IDF I2C APIs.
- **Logging**: Use `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()` from the standard ESP-IDF `esp_log.h`.
- **Unit test support**: Include `#ifdef PROJECT_UNIT_TESTS` block with `ambientLightTestSetUp(void)` to reset all static state.

---

## Deliverables

1. `ambient_light.h` — Public API header with include guard and `@brief`/`@param`/`@return` docs
2. `ambient_light.c` — Full implementation
3. `CMakeLists.txt` — Component build file with `REQUIRES` for all dependencies