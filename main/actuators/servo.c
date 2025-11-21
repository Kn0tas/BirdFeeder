#include "servo.h"
#include "esp_log.h"

static const char *TAG = "servo";

esp_err_t servo_init(void) {
    // TODO: configure LEDC PWM channel for servo control
    ESP_LOGI(TAG, "servo init stub");
    return ESP_OK;
}

esp_err_t servo_set_lid_closed(bool closed) {
    // TODO: drive servo to open/closed angles
    ESP_LOGI(TAG, "servo %s stub", closed ? "closed" : "open");
    return ESP_OK;
}
