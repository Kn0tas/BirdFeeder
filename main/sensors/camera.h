#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t *data;
    size_t size;
    int width;
    int height;
} camera_frame_t;

esp_err_t camera_init(void);
esp_err_t camera_capture(camera_frame_t *out_frame);
