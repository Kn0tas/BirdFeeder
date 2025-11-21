#include "events.h"
#include "esp_log.h"

static const char *TAG = "events";

void events_log(const char *message) {
    if (message) {
        ESP_LOGI(TAG, "%s", message);
    }
}
