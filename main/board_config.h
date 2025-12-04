#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"

// Pin map; adjust as hardware is finalized.
#define PIN_PIR_SENSOR     GPIO_NUM_16
#define PIN_I2C_SDA        GPIO_NUM_8
#define PIN_I2C_SCL        GPIO_NUM_9
#define PIN_SERVO          GPIO_NUM_2
#define PIN_FUEL_ALERT     GPIO_NUM_5  // MAX17048 ALRT (optional)

#define I2C_PORT I2C_NUM_0
#define I2C_CLK_HZ 400000

#define PIR_WAKE_DEBOUNCE_MS 150
#define PIR_RETRIGGER_MS     2000  // minimum quiet time between triggers

// I2C device addresses
#define FRAM_I2C_ADDR     0x50
#define MAX17048_I2C_ADDR 0x36

// Servo configuration (SG90-class)
#define SERVO_PWM_FREQ_HZ     50
#define SERVO_PWM_RES_BITS    14   // use APB clock
#define SERVO_PULSE_MIN_US    500
#define SERVO_PULSE_MAX_US    2500
#define SERVO_PULSE_CLOSED_US 2200
#define SERVO_PULSE_OPEN_US   800
