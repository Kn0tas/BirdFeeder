#include "power_manager.h"

esp_err_t power_manager_init(void) {
  /// @todo Configure deep-sleep wake sources (PIR GPIO, RTC timer) and
  ///       power-rail gating to reduce idle current draw.
  return ESP_OK;
}

esp_err_t power_manager_prepare_sleep(void) {
  /// @todo Shut down peripherals (camera, Wi-Fi, servo) and enter deep sleep
  ///       when idle to save battery.
  return ESP_OK;
}
