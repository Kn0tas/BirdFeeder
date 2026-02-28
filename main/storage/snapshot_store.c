/**
 * @file snapshot_store.c
 * @brief Ring-buffer snapshot storage on SPIFFS.
 *
 * Stores detection JPEG snapshots + metadata on a SPIFFS partition.
 * Old snapshots are evicted when the store is full.
 *
 * File layout on SPIFFS:
 *   /spiffs/meta.bin   — array of snapshot_meta_t (fixed size)
 *   /spiffs/snap_XX.jpg — JPEG files, XX = id
 */

#include "snapshot_store.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

static const char *TAG = "snap_store";

#define MOUNT_POINT "/spiffs"
#define META_PATH MOUNT_POINT "/meta.bin"
#define SNAP_FMT MOUNT_POINT "/snap_%03d.jpg"
#define PATH_BUF_LEN 40

/* ── in-memory ring buffer state ──────────────────────────────────── */

static snapshot_meta_t s_ring[SNAPSHOT_MAX_COUNT];
static int s_count = 0;   /* number of valid entries */
static int s_head = 0;    /* next write position */
static int s_next_id = 0; /* monotonically increasing ID */
static bool s_mounted = false;

/* ── helper: persist metadata ─────────────────────────────────────── */

static void persist_meta(void) {
  FILE *f = fopen(META_PATH, "wb");
  if (!f) {
    ESP_LOGE(TAG, "failed to write meta");
    return;
  }
  /* Write count, head, next_id, then the ring array */
  fwrite(&s_count, sizeof(s_count), 1, f);
  fwrite(&s_head, sizeof(s_head), 1, f);
  fwrite(&s_next_id, sizeof(s_next_id), 1, f);
  fwrite(s_ring, sizeof(s_ring), 1, f);
  fclose(f);
}

static void load_meta(void) {
  FILE *f = fopen(META_PATH, "rb");
  if (!f) {
    /* First boot — no metadata yet */
    ESP_LOGI(TAG, "no existing metadata, starting fresh");
    s_count = 0;
    s_head = 0;
    s_next_id = 0;
    memset(s_ring, 0, sizeof(s_ring));
    return;
  }
  fread(&s_count, sizeof(s_count), 1, f);
  fread(&s_head, sizeof(s_head), 1, f);
  fread(&s_next_id, sizeof(s_next_id), 1, f);
  fread(s_ring, sizeof(s_ring), 1, f);
  fclose(f);

  /* Sanity clamp */
  if (s_count < 0 || s_count > SNAPSHOT_MAX_COUNT) {
    s_count = 0;
  }
  if (s_head < 0 || s_head >= SNAPSHOT_MAX_COUNT) {
    s_head = 0;
  }
  ESP_LOGI(TAG, "loaded %d snapshots, next_id=%d", s_count, s_next_id);
}

/* ── public API ───────────────────────────────────────────────────── */

esp_err_t snapshot_store_init(void) {
  if (s_mounted) {
    return ESP_OK;
  }

  esp_vfs_spiffs_conf_t conf = {
      .base_path = MOUNT_POINT,
      .partition_label = "snapshots",
      .max_files = SNAPSHOT_MAX_COUNT + 2, /* snaps + meta + temp */
      .format_if_mount_failed = true,
  };

  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
    return err;
  }

  size_t total = 0, used = 0;
  esp_spiffs_info("snapshots", &total, &used);
  ESP_LOGI(TAG, "SPIFFS: %u/%u bytes used", (unsigned)used, (unsigned)total);

  s_mounted = true;
  load_meta();
  return ESP_OK;
}

int snapshot_save(const uint8_t *jpeg_data, size_t jpeg_len,
                  const char *species, float confidence) {
  if (!s_mounted || !jpeg_data || jpeg_len == 0) {
    return -1;
  }

  /* If ring is full, delete the oldest snapshot file */
  if (s_count >= SNAPSHOT_MAX_COUNT) {
    char old_path[PATH_BUF_LEN];
    snprintf(old_path, sizeof(old_path), SNAP_FMT, s_ring[s_head].id);
    remove(old_path);
    ESP_LOGI(TAG, "evicted snap id=%d", s_ring[s_head].id);
  }

  /* Assign ID */
  int id = s_next_id++;
  if (s_next_id > 254) {
    s_next_id = 0; /* wrap around */
  }

  /* Write JPEG file */
  char path[PATH_BUF_LEN];
  snprintf(path, sizeof(path), SNAP_FMT, id);
  FILE *f = fopen(path, "wb");
  if (!f) {
    ESP_LOGE(TAG, "failed to write %s", path);
    return -1;
  }
  size_t written = fwrite(jpeg_data, 1, jpeg_len, f);
  fclose(f);
  if (written != jpeg_len) {
    ESP_LOGE(TAG, "partial write %u/%u to %s", (unsigned)written,
             (unsigned)jpeg_len, path);
    remove(path);
    return -1;
  }

  /* Update ring metadata */
  snapshot_meta_t *entry = &s_ring[s_head];
  entry->id = (uint8_t)id;
  strncpy(entry->species, species ? species : "unknown",
          sizeof(entry->species) - 1);
  entry->species[sizeof(entry->species) - 1] = '\0';
  entry->confidence = confidence;
  entry->timestamp = esp_timer_get_time();

  s_head = (s_head + 1) % SNAPSHOT_MAX_COUNT;
  if (s_count < SNAPSHOT_MAX_COUNT) {
    s_count++;
  }

  persist_meta();
  ESP_LOGI(TAG, "saved snap id=%d species=%s conf=%.2f (%u bytes)", id,
           entry->species, confidence, (unsigned)jpeg_len);
  return id;
}

esp_err_t snapshot_get(int id, uint8_t **out_buf, size_t *out_len) {
  if (!s_mounted || !out_buf || !out_len) {
    return ESP_ERR_INVALID_ARG;
  }

  char path[PATH_BUF_LEN];
  snprintf(path, sizeof(path), SNAP_FMT, id);

  FILE *f = fopen(path, "rb");
  if (!f) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Get file size */
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (fsize <= 0) {
    fclose(f);
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)fsize);
  if (!buf) {
    fclose(f);
    return ESP_ERR_NO_MEM;
  }

  size_t rd = fread(buf, 1, (size_t)fsize, f);
  fclose(f);

  if (rd != (size_t)fsize) {
    free(buf);
    return ESP_FAIL;
  }

  *out_buf = buf;
  *out_len = (size_t)fsize;
  return ESP_OK;
}

esp_err_t snapshot_list(snapshot_meta_t *out_list, int *out_count) {
  if (!out_list || !out_count) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Return entries in chronological order (oldest first) */
  int start = (s_count < SNAPSHOT_MAX_COUNT) ? 0 : s_head;
  for (int i = 0; i < s_count; ++i) {
    int idx = (start + i) % SNAPSHOT_MAX_COUNT;
    out_list[i] = s_ring[idx];
  }
  *out_count = s_count;
  return ESP_OK;
}
