/**
 * @file camera_manager.c
 * @brief Mutex-based camera arbitration between streaming and vision tasks.
 */

#include "camera_manager.h"

#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "cam_mgr";

static SemaphoreHandle_t s_mutex = NULL;
static cam_mode_t s_cur_mode = CAM_MODE_VISION;
static bool s_streaming = false;

/**
 * Switch the camera frame size via the OV2640 sensor controls.
 */
static esp_err_t switch_framesize(cam_mode_t mode) {
  /* QVGA (320x240) for streaming — VGA overflows DRAM frame buffers */
  framesize_t fs = (mode == CAM_MODE_STREAM) ? FRAMESIZE_QVGA : FRAMESIZE_QQVGA;

  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor) {
    return ESP_FAIL;
  }
  if (sensor->set_framesize(sensor, fs) != 0) {
    return ESP_FAIL;
  }

  /* Flush stale frames after resolution switch */
  for (int i = 0; i < 3; ++i) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  s_cur_mode = mode;
  ESP_LOGI(TAG, "switched to %s",
           (mode == CAM_MODE_STREAM) ? "QVGA (stream)" : "QQVGA (vision)");
  return ESP_OK;
}

/* ── public API ───────────────────────────────────────────────────── */

esp_err_t cam_mgr_init(void) {
  s_mutex = xSemaphoreCreateMutex();
  if (!s_mutex) {
    ESP_LOGE(TAG, "mutex create failed");
    return ESP_ERR_NO_MEM;
  }
  ESP_LOGI(TAG, "camera manager initialised");
  return ESP_OK;
}

esp_err_t cam_mgr_acquire(cam_mode_t mode, uint32_t timeout_ms) {
  TickType_t ticks =
      (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  if (xSemaphoreTake(s_mutex, ticks) != pdTRUE) {
    ESP_LOGW(TAG, "acquire timed out");
    return ESP_ERR_TIMEOUT;
  }

  s_streaming = (mode == CAM_MODE_STREAM);

  /* Only switch if needed */
  if (s_cur_mode != mode) {
    esp_err_t err = switch_framesize(mode);
    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }
  }

  return ESP_OK;
}

void cam_mgr_release(void) {
  /* If we were streaming, switch back to QQVGA so the smaller frames
   * don't overflow the DRAM buffers while idle. */
  if (s_streaming && s_cur_mode != CAM_MODE_VISION) {
    switch_framesize(CAM_MODE_VISION);
  }
  s_streaming = false;
  xSemaphoreGive(s_mutex);
}

bool cam_mgr_is_streaming(void) { return s_streaming; }
