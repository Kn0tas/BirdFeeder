#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"

// Pin map; adjust as hardware is finalized.
#define PIN_PIR_SENSOR GPIO_NUM_4
#define PIN_I2C_SDA    GPIO_NUM_8
#define PIN_I2C_SCL    GPIO_NUM_9
#define PIN_SERVO      GPIO_NUM_2   // TODO: pick the final servo pin

#define I2C_PORT I2C_NUM_0
#define I2C_CLK_HZ 400000

#define PIR_WAKE_DEBOUNCE_MS 150
