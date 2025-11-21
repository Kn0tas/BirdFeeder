#pragma once

#include "esp_err.h"

esp_err_t power_manager_init(void);
esp_err_t power_manager_prepare_sleep(void);
