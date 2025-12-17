#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    const uint8_t *data;  // pointer to frame buffer owned by driver
    size_t size;
    int width;
    int height;
    bool is_jpeg;
    void *fb_handle;      // opaque handle for returning the buffer
} camera_frame_t;

esp_err_t camera_init(void);
esp_err_t camera_capture(camera_frame_t *out_frame);
void camera_frame_return(camera_frame_t *frame);
void camera_dump_base64(const camera_frame_t *frame);
