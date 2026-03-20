#ifndef CHALLENGE_DEVICE_CONFIG_H_
#define CHALLENGE_DEVICE_CONFIG_H_

#include "driver/gpio.h"

// Challenge-specific GPIO pin definitions
#define CONFIG_PIN_DOOR_SENSOR				GPIO_NUM_5
#define CONFIG_PIN_TEMP_ALERT				GPIO_NUM_17
#define CONFIG_PIN_AMBIENT_LIGHT_ALERT		GPIO_NUM_18

#endif /* CHALLENGE_DEVICE_CONFIG_H_ */
