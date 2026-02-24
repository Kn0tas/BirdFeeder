#include "events.h"
#include "esp_log.h"

static const char *TAG = "events";

/// @todo Evolve into structured event logging with FRAM persistence —
///       store timestamped events (boot, motion, threat, servo actions) in
///       FRAM so they survive deep-sleep cycles and can be uploaded via Wi-Fi.
void events_log(const char *message) {
  if (message) {
    ESP_LOGI(TAG, "%s", message);
  }
}
