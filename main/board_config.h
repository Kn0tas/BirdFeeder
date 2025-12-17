#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"

// Pin map; adjust as hardware is finalized.
#define PIN_PIR_SENSOR     GPIO_NUM_16
#define PIN_I2C_SDA        GPIO_NUM_8
#define PIN_I2C_SCL        GPIO_NUM_9
#define PIN_SERVO          GPIO_NUM_2
#define PIN_FUEL_ALERT     GPIO_NUM_5  // MAX17048 ALRT (optional)

// Camera (OV2640)
#define PIN_CAM_PWDN       GPIO_NUM_4
#define PIN_CAM_RESET      GPIO_NUM_1   // camera reset driven by GPIO1
#define PIN_CAM_XCLK       GPIO_NUM_10
#define PIN_CAM_SIOD       GPIO_NUM_38
#define PIN_CAM_SIOC       GPIO_NUM_39
#define PIN_CAM_VSYNC      GPIO_NUM_6
#define PIN_CAM_HREF       GPIO_NUM_7
#define PIN_CAM_PCLK       GPIO_NUM_12
// Data lines labeled D2-D9 on the breakout
#define PIN_CAM_D2         GPIO_NUM_11
#define PIN_CAM_D3         GPIO_NUM_13
#define PIN_CAM_D4         GPIO_NUM_18
#define PIN_CAM_D5         GPIO_NUM_17
#define PIN_CAM_D6         GPIO_NUM_14
#define PIN_CAM_D7         GPIO_NUM_15
#define PIN_CAM_D8         GPIO_NUM_48
#define PIN_CAM_D9         GPIO_NUM_47

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
