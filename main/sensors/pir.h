#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t pir_init(void);
esp_err_t pir_wait_for_motion(TickType_t timeout_ticks);
bool pir_motion_detected(void);
