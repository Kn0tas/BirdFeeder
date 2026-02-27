#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

/** Maximum number of snapshots kept (ring-buffer, oldest deleted first). */
#define SNAPSHOT_MAX_COUNT 15

/** Metadata for a single detection snapshot. */
typedef struct {
  uint8_t id;        /**< Unique ID (0-254, wraps) */
  char species[16];  /**< e.g. "crow", "magpie", "squirrel" */
  float confidence;  /**< Classification confidence */
  int64_t timestamp; /**< Microseconds since boot (esp_timer_get_time) */
} snapshot_meta_t;

/**
 * @brief Initialise the snapshot store (mount SPIFFS partition).
 * @return ESP_OK on success.
 */
esp_err_t snapshot_store_init(void);

/**
 * @brief Save a JPEG snapshot with detection metadata.
 *
 * If the store is full, the oldest snapshot is evicted first.
 *
 * @param jpeg_data  Pointer to JPEG data.
 * @param jpeg_len   Length of JPEG data.
 * @param species    Species string (e.g. "crow").
 * @param confidence Classification confidence.
 * @return ID of the saved snapshot, or -1 on error.
 */
int snapshot_save(const uint8_t *jpeg_data, size_t jpeg_len,
                  const char *species, float confidence);

/**
 * @brief Read a snapshot JPEG by ID.
 *
 * @param id        Snapshot ID.
 * @param out_buf   Output buffer (caller must free).
 * @param out_len   Output length.
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t snapshot_get(int id, uint8_t **out_buf, size_t *out_len);

/**
 * @brief Get the list of stored snapshot metadata.
 *
 * @param out_list  Output array (caller provides, length SNAPSHOT_MAX_COUNT).
 * @param out_count Actual number of entries written.
 * @return ESP_OK on success.
 */
esp_err_t snapshot_list(snapshot_meta_t *out_list, int *out_count);
