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
#include "max17048.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";

static void handle_motion_event(void) {
    events_log("motion detected");
    if (servo_set_lid_closed(true) != ESP_OK) {
        ESP_LOGE(TAG, "servo close failed");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (servo_set_lid_closed(false) != ESP_OK) {
        ESP_LOGE(TAG, "servo open failed");
    }

    // Capture a frame on motion for validation/telemetry
    camera_frame_t frame = {0};
    if (camera_capture(&frame) == ESP_OK) {
        ESP_LOGI(TAG, "motion capture %dx%d (%u bytes, jpeg=%d)", frame.width, frame.height, (unsigned)frame.size, frame.is_jpeg);
        camera_frame_return(&frame);
    } else {
        ESP_LOGW(TAG, "motion capture failed");
    }
}

void app_main(void) {
    // NVS needed for Wi-Fi; erase/reinit if needed
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs_ret);
    }

    ESP_ERROR_CHECK(power_manager_init());
    ESP_ERROR_CHECK(fram_init());
    ESP_ERROR_CHECK(max17048_init());
    ESP_ERROR_CHECK(pir_init());
    ESP_ERROR_CHECK(servo_init());
    ESP_ERROR_CHECK(camera_init());
    ESP_ERROR_CHECK(vision_init());
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(ota_init());

    max17048_reading_t fuel = {0};
    if (max17048_read(&fuel) == ESP_OK) {
        ESP_LOGI(TAG, "fuel gauge: %.2f%%, %.3fV", fuel.soc_percent, fuel.voltage_v);
    } else {
        ESP_LOGW(TAG, "fuel gauge read failed");
    }

    // Start with lid open
    if (servo_set_lid_closed(false) != ESP_OK) {
        ESP_LOGE(TAG, "servo initial open failed");
    }
    events_log("boot");

    // One-shot capture at boot for wiring validation (no base64 dump to reduce load)
    vTaskDelay(pdMS_TO_TICKS(200));  // allow sensor to settle
    camera_frame_t frame = {0};
    if (camera_capture(&frame) == ESP_OK) {
        ESP_LOGI(TAG, "boot capture %dx%d (%u bytes, jpeg=%d)", frame.width, frame.height, (unsigned)frame.size, frame.is_jpeg);
        camera_frame_return(&frame);
    } else {
        ESP_LOGW(TAG, "boot capture failed");
    }

    while (true) {
        ESP_LOGI(TAG, "waiting for PIR motion");
        pir_wait_for_motion(portMAX_DELAY);
        handle_motion_event();
        power_manager_prepare_sleep();
        vTaskDelay(pdMS_TO_TICKS(PIR_RETRIGGER_MS));
    }
}
