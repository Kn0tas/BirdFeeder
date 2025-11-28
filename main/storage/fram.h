#pragma once

#include <stddef.h>
#include "esp_err.h"

esp_err_t fram_init(void);
esp_err_t fram_write_config(const void *data, size_t len);
esp_err_t fram_read_config(void *data, size_t len);
