#pragma once

#include "esp_err.h"

typedef struct {
    float voltage_v;
    float soc_percent;
} max17048_reading_t;

esp_err_t max17048_init(void);
esp_err_t max17048_quickstart(void);
esp_err_t max17048_read(max17048_reading_t *out);
