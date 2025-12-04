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
}

void app_main(void) {
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

    while (true) {
        ESP_LOGI(TAG, "waiting for PIR motion");
        pir_wait_for_motion(portMAX_DELAY);
        handle_motion_event();
        power_manager_prepare_sleep();
        vTaskDelay(pdMS_TO_TICKS(PIR_RETRIGGER_MS));
    }
}
