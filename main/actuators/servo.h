#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t servo_init(void);
esp_err_t servo_set_lid_closed(bool closed);
