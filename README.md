# Embedded Software Engineer — Coding Challenge

## Overview

This is a **take-home coding challenge** for embedded software engineer candidates.
It evaluates your ability to work in a real ESP-IDF / FreeRTOS firmware codebase: reading and
understanding conventions, debugging concurrency and platform issues, and implementing a new
hardware-driver component from a specification.

## Structure

```
root/
├── CHALLENGE.md                   ← Start here — full task description
├── BUILD.md                       ← How to build and run unit tests locally
├── coding_rules/
│   └── README_C_code_rules.md     ← Project coding conventions (your style bible)
├── test_support/                  ← Mock infrastructure for unit tests
│   ├── unit_tests_definitions.h
│   ├── device_config/             ← GPIO pin definitions
│   └── mocks/                     ← FreeRTOS, driver, hw_i2c mocks
├── reference/
│   ├── door_sensor.h              ← Working reference component (header)
│   ├── door_sensor.c              ← Working reference component (source)
│   ├── CMakeLists.txt             ← Component build file example
│   └── unit_tests/                ← Reference unit test structure
│       ├── CMakeLists.txt
│       └── main/
│           ├── CMakeLists.txt
│           └── main.c
├── part1_bug_review/
│   ├── temperature_sensor.h       ← Buggy component — find and fix issues
│   ├── temperature_sensor.c
│   └── CMakeLists.txt
├── part2_new_component/
│   └── SPEC.md                    ← Sensor specification to implement
└── part3_unit_tests/
    └── README.md                  ← Unit test requirements
```

## Rules

1. **Read `CHALLENGE.md`** for detailed instructions on each part.
2. **Study `coding_rules/README_C_code_rules.md`** before writing any code.
3. **Use the `reference/` component** as your pattern guide.
4. All code must follow the project's conventions exactly.
5. You may use documentation (ESP-IDF docs, FreeRTOS docs).

## Submission

Deliver a zip archive or git repository containing:

- `part1_bug_review/REVIEW.md` — Your bug report with explanations and fixes
- `part2_new_component/ambient_light.h`, `ambient_light.c`, `CMakeLists.txt`
- `part3_unit_tests/` — Your unit test files
- (Optional) `DECISIONS.md` — Design decisions and trade-offs

Good luck!
