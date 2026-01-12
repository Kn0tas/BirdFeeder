#pragma once

#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// Placeholder for embedded TFLite Micro model. Replace with real model data.
extern const unsigned char *g_model_tflite;
extern size_t g_model_tflite_len;

void vision_model_data_init(void);

#ifdef __cplusplus
}
#endif
