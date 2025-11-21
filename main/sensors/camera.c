#include "camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

esp_err_t camera_init(void) {
    // TODO: configure OV2640 driver once hardware is connected
    ESP_LOGI(TAG, "camera init stub");
    return ESP_OK;
}

esp_err_t camera_capture(camera_frame_t *out_frame) {
    if (!out_frame) {
        return ESP_ERR_INVALID_ARG;
    }
    // Placeholder frame metadata; real implementation will fill buffer
    out_frame->data = NULL;
    out_frame->size = 0;
    out_frame->width = 0;
    out_frame->height = 0;
    ESP_LOGI(TAG, "capture stub");
    return ESP_OK;
}
