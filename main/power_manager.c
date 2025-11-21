#include "power_manager.h"

esp_err_t power_manager_init(void) {
    // TODO: configure deep sleep sources (PIR GPIO, timer, etc.) and power rails
    return ESP_OK;
}

esp_err_t power_manager_prepare_sleep(void) {
    // TODO: enter deep sleep when idle to save battery
    return ESP_OK;
}
