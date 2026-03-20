# Part 3 — Unit Tests for Ambient Light Sensor

## Overview

Write host-based unit tests for the ambient light sensor component you implemented
in Part 2. Tests run on Linux (no hardware required) using the **Unity** assertion
framework and **CMock** for mocking all external dependencies.

---

## Directory Structure

Create the following files in this directory:

```
part3_unit_tests/
├── CMakeLists.txt          ← Standalone CMake project
├── sdkconfig.defaults      ← Copy from test_support/sdkconfig.defaults
└── main/
    ├── CMakeLists.txt      ← Component registration + coverage flags
    └── main.c              ← Test cases + Unity test runner
```

---

## CMakeLists.txt (top-level)

Use the same pattern as `reference/unit_tests/CMakeLists.txt`:

- Set `COMPONENTS` to `main`
- Set `COMPONENT_NAME` to your component name
- Include the unit test definitions header
- Add mock component directories for: `freertos`, `driver`, `device_config`,
  and `hw_i2c`

---

## CMakeLists.txt (main/)

Use the same pattern as `reference/unit_tests/main/CMakeLists.txt`:

- Glob component sources from the parent directory
- Register with `idf_component_register`
- Add `--coverage` compile and link flags

---

## Test Requirements

### Minimum test coverage

1. **Init success** — Verify GPIO config, semaphore creation, I2C sensor configuration
   sequence, and task creation with correct parameters
2. **Init failure** — Task creation returns `pdFALSE`, verify `ESP_FAIL` returned
3. **Read success** — Mock I2C read to return test data, verify correct lux value
4. **Read with NULL pointer** — Verify `ESP_ERR_INVALID_ARG`
5. **Alert flow** — Capture ISR and task function pointers via `_StubWithCallback`,
   then drive the task: verify I2C INT_STATUS read (clear interrupt), I2C data read,
   and callback invocation
6. **Set threshold** — Verify correct I2C write with little-endian byte order
7. **Sleep / Wake** — Verify correct CONTROL register values written

### Mocking pattern

```c
// Include auto-generated mock headers
#include "Mockgpio.h"
#include "Mockqueue.h"
#include "Mocktask.h"
#include "Mockhw_i2c.h"
```

### Key CMock patterns to use

| Pattern | When to use |
| --- | --- |
| `func_ExpectAndReturn(args, retval)` | When you want to verify exact arguments |
| `func_IgnoreAndReturn(retval)` | When you don't care about arguments for that call |
| `func_IgnoreArg_paramName()` | When you want to verify most args but ignore one |
| `func_StubWithCallback(your_func)` | When you need to capture function pointers (ISR, task) |
| `func_ReturnThruPtr_paramName(&val)` | When you need to set an output parameter |
| `func_ExpectAnyArgsAndReturn(retval)` | When you verify call happens but not specific args |

### Test runner

```c
int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(testAmbientLightInitSuccess);
    RUN_TEST(testAmbientLightInitTaskFail);
    // ... more tests ...

    int lFailures = UNITY_END();
    return lFailures;
}
```

---

## Building

Copy `sdkconfig.defaults` from `test_support/` and build:

```bash
cp ../../test_support/sdkconfig.defaults .
idf.py --preview set-target linux
idf.py build
./build/unit_tests.elf
```

See [BUILD.md](../BUILD.md) for full details.

## Tips

- Study `reference/unit_tests/main/main.c` carefully — it demonstrates all the patterns.
- Use a `setUp()` function to reset state and ignore log calls before each test.
- Use `_StubWithCallback` on `xTaskCreatePinnedToCore` to capture the task function pointer,
  then call it directly in your test (it will run once and exit due to `PROJECT_UNIT_TESTS`).
- Similarly, use `_StubWithCallback` on `gpio_isr_handler_add` to capture the ISR pointer.
