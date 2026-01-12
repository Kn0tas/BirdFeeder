#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "actuators/servo.h"
#include "board_config.h"
#include "comms/ota.h"
#include "comms/wifi.h"
#include "esp_psram.h"
#include "i2c_bus.h"
#include "logging/events.h"
#include "max17048.h"
#include "nvs_flash.h"
#include "power_manager.h"
#include "sensors/camera.h"
#include "sensors/pir.h"
#include "storage/fram.h"
#include "vision/vision.h"

static const char *TAG = "app_main";

static const char *get_vision_kind_str(vision_kind_t kind) {
  switch (kind) {
  case VISION_RESULT_CROW:
    return "CROW";
  case VISION_RESULT_SQUIRREL:
    return "SQUIRREL";
  case VISION_RESULT_MAGPIE:
    return "MAGPIE";
  case VISION_RESULT_BIRD:
    return "BIRD";
  case VISION_RESULT_RAT:
    return "RAT";
  case VISION_RESULT_OTHER:
    return "OTHER";
  case VISION_RESULT_UNKNOWN:
  default:
    return "UNKNOWN";
  }
}

static void handle_motion_event(void) {
  events_log("motion detected");
  const TickType_t detection_window = pdMS_TO_TICKS(5000);
  const float threat_thresh = 0.70f;
  const int consecutive_needed = 2;
  int consecutive_hits = 0;
  bool lid_closed = false;
  bool discard_first = true;
  TickType_t start = xTaskGetTickCount();

  while ((xTaskGetTickCount() - start) < detection_window) {
    camera_frame_t frame = {0};
    if (camera_capture(&frame) != ESP_OK) {
      ESP_LOGW(TAG, "motion capture failed");
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (discard_first) {
      camera_frame_return(&frame);
      discard_first = false;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    vision_result_t res = {0};
    if (vision_classify(&frame, &res) != ESP_OK) {
      ESP_LOGW(TAG, "vision classify failed");
      camera_frame_return(&frame);
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    ESP_LOGI(TAG, "detected %s (conf=%.2f)", get_vision_kind_str(res.kind),
             res.confidence);
    camera_frame_return(&frame);

    bool is_threat =
        (res.kind == VISION_RESULT_CROW || res.kind == VISION_RESULT_SQUIRREL ||
         res.kind == VISION_RESULT_RAT) &&
        res.confidence >= threat_thresh;

    if (is_threat) {
      consecutive_hits++;
      ESP_LOGI(TAG, "threat detected (%d/%d) kind=%d conf=%.2f",
               consecutive_hits, consecutive_needed, res.kind, res.confidence);
      if (!lid_closed && consecutive_hits >= consecutive_needed) {
        if (servo_set_lid_closed(true) == ESP_OK) {
          lid_closed = true;
          ESP_LOGI(TAG, "servo closed due to threat");
        } else {
          ESP_LOGE(TAG, "servo close failed");
        }
      }
    } else {
      consecutive_hits = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  if (lid_closed) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (servo_set_lid_closed(false) != ESP_OK) {
      ESP_LOGE(TAG, "servo reopen failed");
    } else {
      ESP_LOGI(TAG, "servo reopened after threat window");
    }
  }
}

void app_main(void) {
  // NVS needed for Wi-Fi; erase/reinit if needed
  esp_err_t nvs_ret = nvs_flash_init();
  if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  } else {
    ESP_ERROR_CHECK(nvs_ret);
  }

#if CONFIG_SPIRAM
  size_t psram_size = esp_psram_get_size();
  if (psram_size > 0) {
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)psram_size);
  } else {
    ESP_LOGW(TAG, "PSRAM not detected");
  }
#else
  ESP_LOGW(TAG, "PSRAM not enabled in config");
#endif

  ESP_ERROR_CHECK(power_manager_init());
  ESP_ERROR_CHECK(i2c_bus_init());
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
    ESP_LOGI(TAG, "fuel gauge: %.2f%%, %.3fV", fuel.soc_percent,
             fuel.voltage_v);
  } else {
    ESP_LOGW(TAG, "fuel gauge read failed");
  }

  // Start with lid open
  if (servo_set_lid_closed(false) != ESP_OK) {
    ESP_LOGE(TAG, "servo initial open failed");
  }
  events_log("boot");

  // One-shot capture at boot for wiring validation (no base64 dump to reduce
  // load)
  vTaskDelay(pdMS_TO_TICKS(200)); // allow sensor to settle
  camera_frame_t frame = {0};
  if (camera_capture(&frame) == ESP_OK) {
    ESP_LOGI(TAG, "boot capture %dx%d (%u bytes, jpeg=%d)", frame.width,
             frame.height, (unsigned)frame.size, frame.is_jpeg);
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
