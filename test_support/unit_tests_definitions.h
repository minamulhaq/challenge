#pragma once

#define PROJECT_UNIT_TESTS

#define configSUPPORT_STATIC_ALLOCATION     1
#define portYIELD_FROM_ISR()

#ifndef CONFIG_FREERTOS_HZ
#define CONFIG_FREERTOS_HZ                  1000
#endif

#ifndef CONFIG_FREERTOS_MAX_TASK_NAME_LEN
#define CONFIG_FREERTOS_MAX_TASK_NAME_LEN   16
#endif
