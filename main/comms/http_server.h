#pragma once

#include "esp_err.h"

/**
 * @brief Start the HTTP server.
 *
 * Registers endpoints:
 *   GET /stream?mode=live   — MJPEG stream at VGA (640×480)
 *   GET /stream?mode=ai     — MJPEG stream at 96×96 (AI view) with X-Detection header
 *   GET /capture            — single JPEG snapshot
 *   GET /status             — JSON device status
 *
 * @return ESP_OK on success.
 */
esp_err_t http_server_start(void);
