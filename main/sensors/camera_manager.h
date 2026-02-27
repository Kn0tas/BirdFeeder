#pragma once

#include "esp_err.h"
#include <stdbool.h>


/**
 * @brief Camera usage mode.
 */
typedef enum {
  CAM_MODE_VISION, /**< QQVGA 160×120 for TFLite classification */
  CAM_MODE_STREAM, /**< VGA 640×480 for live MJPEG streaming */
} cam_mode_t;

/**
 * @brief Initialise the camera manager (creates mutex).
 * Must be called after camera_init().
 */
esp_err_t cam_mgr_init(void);

/**
 * @brief Acquire the camera for a given mode.
 *
 * Blocks until the camera is available. Sets the resolution
 * appropriate for the requested mode.
 *
 * @param mode  Desired camera mode.
 * @param timeout_ms  Maximum wait time in ms (0 = forever).
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if timed out.
 */
esp_err_t cam_mgr_acquire(cam_mode_t mode, uint32_t timeout_ms);

/**
 * @brief Release the camera so other tasks can use it.
 */
void cam_mgr_release(void);

/**
 * @brief Check if the camera is currently acquired for streaming.
 * @return true if a streaming client holds the camera.
 */
bool cam_mgr_is_streaming(void);
