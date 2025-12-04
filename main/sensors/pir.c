#include "pir.h"
#include "esp_log.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "pir";

esp_err_t pir_init(void) {
    gpio_reset_pin(PIN_PIR_SENSOR);
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_PIR_SENSOR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t pir_wait_for_motion(TickType_t timeout_ticks) {
    const TickType_t poll_delay = pdMS_TO_TICKS(20);
    TickType_t start = xTaskGetTickCount();

    // Require low before looking for a rising edge (AM312 idle low)
    while (gpio_get_level(PIN_PIR_SENSOR)) {
        if (timeout_ticks != portMAX_DELAY && (xTaskGetTickCount() - start) >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(poll_delay);
    }

    while (true) {
        if (gpio_get_level(PIN_PIR_SENSOR)) {
            TickType_t stable_start = xTaskGetTickCount();
            while (gpio_get_level(PIN_PIR_SENSOR)) {
                if ((xTaskGetTickCount() - stable_start) >= pdMS_TO_TICKS(PIR_WAKE_DEBOUNCE_MS)) {
                    return ESP_OK;
                }
                vTaskDelay(poll_delay);
            }
        }
        if (timeout_ticks != portMAX_DELAY && (xTaskGetTickCount() - start) >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(poll_delay);
    }
}

bool pir_motion_detected(void) {
    return gpio_get_level(PIN_PIR_SENSOR) == 1;
}
