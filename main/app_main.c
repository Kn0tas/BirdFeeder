#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board_config.h"
#include "power_manager.h"
#include "sensors/pir.h"
#include "sensors/camera.h"
#include "vision/vision.h"
#include "actuators/servo.h"
#include "comms/wifi.h"
#include "comms/ota.h"
#include "storage/fram.h"
#include "logging/events.h"

static const char *TAG = "app_main";

static void handle_motion_event(void) {
    camera_frame_t frame = {0};
    if (camera_capture(&frame) != ESP_OK) {
        ESP_LOGW(TAG, "capture failed");
        return;
    }

    vision_result_t result = {0};
    if (vision_classify(&frame, &result) != ESP_OK) {
        ESP_LOGW(TAG, "classification failed");
        return;
    }

    switch (result.kind) {
        case VISION_RESULT_SQUIRREL:
            events_log("squirrel detected");
            servo_set_lid_closed(true);
            break;
        case VISION_RESULT_BIRD:
            events_log("bird detected");
            servo_set_lid_closed(false);
            break;
        default:
            events_log("motion detected (unknown)");
            break;
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(power_manager_init());
    ESP_ERROR_CHECK(fram_init());
    ESP_ERROR_CHECK(pir_init());
    ESP_ERROR_CHECK(servo_init());
    ESP_ERROR_CHECK(camera_init());
    ESP_ERROR_CHECK(vision_init());
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(ota_init());

    events_log("boot");

    while (true) {
        ESP_LOGI(TAG, "waiting for PIR motion");
        pir_wait_for_motion(portMAX_DELAY);
        handle_motion_event();
        power_manager_prepare_sleep();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
