#include "wifi.h"
#include "esp_log.h"

static const char *TAG = "wifi";

esp_err_t wifi_init(void) {
    // TODO: add provisioning + STA connection logic
    ESP_LOGI(TAG, "wifi init stub");
    return ESP_OK;
}
