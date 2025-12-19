#include "model_data.h"

// Linker symbols for the embedded TFLite file (added via EMBED_FILES in CMakeLists.txt)
extern const unsigned char _binary_model_int8_tflite_start[] asm("_binary_model_int8_tflite_start");
extern const unsigned char _binary_model_int8_tflite_end[] asm("_binary_model_int8_tflite_end");

const unsigned char *g_model_tflite = NULL;
size_t g_model_tflite_len = 0;

void vision_model_data_init(void) {
    g_model_tflite = _binary_model_int8_tflite_start;
    g_model_tflite_len = (size_t)(_binary_model_int8_tflite_end - _binary_model_int8_tflite_start);
}
