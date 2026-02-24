#include "ota.h"
#include "esp_log.h"

static const char *TAG = "ota";

esp_err_t ota_init(void) {
  /// @todo Implement HTTPS OTA update flow — check for firmware updates on
  ///       boot or on a periodic timer, download, validate, and apply.
  ESP_LOGI(TAG, "ota init stub");
  return ESP_OK;
}
