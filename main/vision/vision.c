#include "vision.h"
#include "esp_log.h"
#include "vision/model_data.h"

static const char *TAG = "vision";

esp_err_t vision_init(void) {
    if (g_model_tflite_len == 0) {
        ESP_LOGW(TAG, "no model embedded; vision will always return UNKNOWN");
    } else {
        ESP_LOGI(TAG, "vision model loaded (%u bytes)", (unsigned)g_model_tflite_len);
    }
    return ESP_OK;
}

esp_err_t vision_classify(const camera_frame_t *frame, vision_result_t *result) {
    if (!frame || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_model_tflite_len == 0) {
        result->kind = VISION_RESULT_UNKNOWN;
        result->confidence = 0.0f;
        return ESP_OK;
    }
    // TODO: run TFLite Micro model once integrated.
    result->kind = VISION_RESULT_UNKNOWN;
    result->confidence = 0.0f;
    return ESP_OK;
}
