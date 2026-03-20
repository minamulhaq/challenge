# Building & Running Unit Tests

This document explains how to build and run the unit tests locally.

## Prerequisites

- **ESP-IDF v5.x** installed and activated (`idf.py` available in PATH)
- Linux host target support (included with standard ESP-IDF installation)

If you haven't set up ESP-IDF yet, follow [Espressif's Getting Started guide](https://docs.espressif.com/projects/esp-idf/en/v5.0.9/esp32s3/get-started/index.html).

## Building & Running Tests

### Reference component (Part 2 example: `door_sensor`)

```bash
cd reference/unit_tests
idf.py --preview set-target linux
idf.py build
./build/unit_tests.elf
```

### Your component (Part 2 + Part 3)

Create a `unit_tests/` directory inside your component with the same structure
as the reference. Copy the required sdkconfig defaults, then build:

```bash
cd <your_component>/unit_tests
cp ../../test_support/sdkconfig.defaults .
idf.py --preview set-target linux
idf.py build
./build/unit_tests.elf
```

> **Important:** The `sdkconfig.defaults` file is required. Without it the build will
> fail with missing `esp_system` / `esp_ipc` headers. A template is provided in
> `test_support/sdkconfig.defaults` — copy it into your `unit_tests/` directory.

## Project Structure

Tests use mocks from `test_support/` which provides:

- **`test_support/mocks/freertos/`** — FreeRTOS task, queue, event_groups, timers mocks
- **`test_support/mocks/driver/`** — GPIO driver mock
- **`test_support/mocks/hw_i2c/`** — I2C abstraction mock (`i2cInit`, `i2cRead`, `i2cWrite`)
- **`test_support/device_config/`** — GPIO pin definitions for the challenge
- **`test_support/unit_tests_definitions.h`** — Common test build definitions

## CMakeLists Structure

Each `unit_tests/CMakeLists.txt` (top-level) should follow this pattern:

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main)
set(COMPONENT_NAME "your_component")
set(CHALLENGE_DIR "${CMAKE_SOURCE_DIR}/../..")

add_definitions(-include "${CHALLENGE_DIR}/test_support/unit_tests_definitions.h")

list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/freertos")
list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/driver")
list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/device_config")
list(APPEND EXTRA_COMPONENT_DIRS "${CHALLENGE_DIR}/test_support/mocks/hw_i2c")

project(unit_tests)
```

Each `unit_tests/main/CMakeLists.txt` should follow:

```cmake
set(component_dir "${CMAKE_CURRENT_SOURCE_DIR}/../..")

file(GLOB component_srcs "${component_dir}/*.c")

idf_component_register(
    SRCS "main.c" "${component_srcs}"
    INCLUDE_DIRS "." "${component_dir}"
    REQUIRES cmock driver device_config hw_i2c
)

target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
target_link_libraries(${COMPONENT_LIB} --coverage)
```

## Troubleshooting

- **`idf.py: command not found`** — Run `. $IDF_PATH/export.sh` to activate ESP-IDF.
- **`set-target linux` fails** — Ensure your ESP-IDF version supports the Linux target
  (v5.0+). The `--preview` flag is required.
- **Build errors about missing headers** — Verify your `CMakeLists.txt` paths point to
  `${CHALLENGE_DIR}/test_support/...` correctly.
- **Clean rebuild** — Delete the `build/` directory and re-run `idf.py build`.
