#pragma once

#include <stdint.h>
#include <stddef.h>

// Placeholder for embedded TFLite Micro model. Replace with real model data.
extern const unsigned char *g_model_tflite;
extern size_t g_model_tflite_len;

void vision_model_data_init(void);
