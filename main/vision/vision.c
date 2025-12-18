#include "vision.h"
#include "esp_log.h"

static const char *TAG = "vision";

esp_err_t vision_init(void) {
    // TODO: load lightweight model or heuristic pipeline
    ESP_LOGI(TAG, "vision init stub");
    return ESP_OK;
}

esp_err_t vision_classify(const camera_frame_t *frame, vision_result_t *result) {
    if (!frame || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    // TODO: run TFLite Micro model once integrated.
    result->kind = VISION_RESULT_UNKNOWN;
    result->confidence = 0.0f;
    return ESP_OK;
}
