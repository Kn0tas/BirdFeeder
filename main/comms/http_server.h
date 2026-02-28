#pragma once

#include "esp_err.h"

/**
 * @brief Start the HTTP server.
 *
 * Registers endpoints:
 *   GET /stream?mode=live   — MJPEG stream at QVGA (320×240)
 *   GET /stream?mode=ai     — MJPEG stream at 96×96 (AI view) with X-Detection
 * header GET /capture            — single JPEG snapshot GET /status — JSON
 * device status GET /events             — JSON array of detection events GET
 * /events/<id>.jpg    — individual detection snapshot WS  /ws                 —
 * WebSocket for real-time detection alerts
 *
 * @return ESP_OK on success.
 */
esp_err_t http_server_start(void);

/**
 * @brief Broadcast a detection event to all connected WebSocket clients.
 *
 * Sends JSON:
 * {"type":"detection","species":"crow","confidence":0.85,"snap_id":3}
 *
 * @param species   Species string (e.g. "crow", "magpie", "squirrel").
 * @param confidence Classification confidence (0.0–1.0).
 * @param snap_id   Snapshot ID from snapshot_save(), or -1 if none.
 */
void http_server_notify_detection(const char *species, float confidence,
                                  int snap_id);
