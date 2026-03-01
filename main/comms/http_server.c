/**
 * @file http_server.c
 * @brief HTTP server with MJPEG streaming, snapshot, and status endpoints.
 *
 * Endpoints:
 *   GET /stream?mode=live  — MJPEG at VGA (640×480), ~10-15 fps
 *   GET /stream?mode=ai    — MJPEG at 96×96 (AI preprocessed view)
 *   GET /capture           — single JPEG frame
 *   GET /status            — JSON {battery_v, battery_pct, uptime_s}
 *   GET /events            — JSON array of detection events
 *   GET /events/<id>.jpg  — individual detection snapshot JPEG
 */

#include "http_server.h"

#include <string.h>
#include <time.h>

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "sensors/camera.h"
#include "sensors/camera_manager.h"
#include "sensors/max17048.h"
#include "storage/snapshot_store.h"
#include "vision/vision.h"

static const char *TAG = "httpd";

/* ── WebSocket client tracking ───────────────────────────────────────── */

#define WS_MAX_CLIENTS 4
static int s_ws_fds[WS_MAX_CLIENTS];
static int s_ws_count = 0;
static httpd_handle_t s_server = NULL;

/* ── stream constants ────────────────────────────────────────────────── */

#define MJPEG_BOUNDARY "birdfeeder"
#define MJPEG_CT "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY
#define PART_HDR_FMT                                                           \
  "\r\n--" MJPEG_BOUNDARY                                                      \
  "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n"
#define AI_PART_HDR_FMT                                                        \
  "\r\n--" MJPEG_BOUNDARY                                                      \
  "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\nX-Detection: %s\r\n"
#define PART_HDR_END "\r\n"

/* AI crop target (must match vision model input) */
#define AI_VIEW_W 96
#define AI_VIEW_H 96

/* Maximum consecutive capture failures before aborting stream */
#define MAX_STREAM_ERRORS 10

/* ── helpers ─────────────────────────────────────────────────────────── */

/**
 * Centre-crop + nearest-neighbour resize an RGB888 buffer to AI_VIEW_W ×
 * AI_VIEW_H. Returns a heap-allocated RGB888 buffer that the caller must
 * free().
 */
static uint8_t *crop_resize_rgb(const uint8_t *rgb, int src_w, int src_h) {
  const size_t out_size = (size_t)AI_VIEW_W * AI_VIEW_H * 3;
  uint8_t *out = (uint8_t *)malloc(out_size);
  if (!out) {
    return NULL;
  }

  const int crop = (src_w < src_h) ? src_w : src_h;
  const int crop_x = (src_w - crop) / 2;
  const int crop_y = (src_h - crop) / 2;

  uint8_t *dst = out;
  for (int y = 0; y < AI_VIEW_H; ++y) {
    int sy = crop_y + (y * crop) / AI_VIEW_H;
    for (int x = 0; x < AI_VIEW_W; ++x) {
      int sx = crop_x + (x * crop) / AI_VIEW_W;
      size_t si = ((size_t)sy * (size_t)src_w + (size_t)sx) * 3;
      *dst++ = rgb[si];
      *dst++ = rgb[si + 1];
      *dst++ = rgb[si + 2];
    }
  }
  return out;
}

/**
 * Encode a raw RGB888 buffer to JPEG.
 * Returns a heap-allocated JPEG buffer; sets *out_len. Caller must free().
 */
static uint8_t *encode_rgb_to_jpeg(const uint8_t *rgb, int w, int h,
                                   int quality, size_t *out_len) {
  uint8_t *jpg = NULL;
  /* fmt2jpg may take non-const input on some ESP-IDF versions */
  if (!fmt2jpg((uint8_t *)rgb, (size_t)w * h * 3, w, h, PIXFORMAT_RGB888,
               quality, &jpg, out_len)) {
    return NULL;
  }
  return jpg;
}

/* ── GET /stream ─────────────────────────────────────────────────────── */

/**
 * Live MJPEG stream handler (mode=live).
 * Switches camera to VGA, continuously captures and streams JPEG frames.
 */
static esp_err_t stream_live_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "stream live: client connected");

  /* Acquire camera in streaming mode (VGA) */
  if (cam_mgr_acquire(CAM_MODE_STREAM, 5000) != ESP_OK) {
    ESP_LOGE(TAG, "failed to acquire camera for stream");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, MJPEG_CT);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part_hdr[128];
  int err_count = 0;

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      ESP_LOGW(TAG, "stream: capture failed");
      if (++err_count > MAX_STREAM_ERRORS) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    err_count = 0;

    int hdr_len =
        snprintf(part_hdr, sizeof(part_hdr), PART_HDR_FMT, (unsigned)fb->len);

    esp_err_t res = httpd_resp_send_chunk(req, part_hdr, hdr_len);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, PART_HDR_END, strlen(PART_HDR_END));
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, (int)fb->len);
    }
    esp_camera_fb_return(fb);

    if (res != ESP_OK) {
      /* Client disconnected */
      break;
    }

    /* Throttle to ~20 fps to prevent FB-OVF on slow connections */
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  ESP_LOGI(TAG, "stream live: client disconnected");

  /* Release camera so detection task can use it */
  cam_mgr_release();

  return ESP_OK;
}

/**
 * AI-view MJPEG stream handler (mode=ai).
 * Camera at QQVGA, decodes to RGB, crops to 96×96, re-encodes, adds
 * classification result in X-Detection header, and streams.
 */
static esp_err_t stream_ai_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "stream ai: client connected");

  /* Acquire camera in vision mode (QQVGA) */
  if (cam_mgr_acquire(CAM_MODE_VISION, 5000) != ESP_OK) {
    ESP_LOGE(TAG, "failed to acquire camera for AI stream");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, MJPEG_CT);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part_hdr[256];
  int err_count = 0;

  while (true) {
    camera_frame_t frame = {0};
    if (camera_capture(&frame) != ESP_OK) {
      ESP_LOGW(TAG, "ai stream: capture failed");
      if (++err_count > MAX_STREAM_ERRORS) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    err_count = 0;

    /* Run classification on the frame — this is CPU-heavy (seconds).
     * Add the httpd task to the WDT, reset it around inference,
     * and yield briefly to let IDLE run. */
    esp_task_wdt_add(NULL);
    vision_result_t vr = {0};
    vision_classify(&frame, &vr);
    esp_task_wdt_reset();
    esp_task_wdt_delete(NULL);
    vTaskDelay(1); /* yield to IDLE task */

    /* Decode JPEG → RGB */
    const int src_w = frame.width;
    const int src_h = frame.height;
    const size_t rgb_size = (size_t)src_w * src_h * 3;
    uint8_t *rgb = (uint8_t *)malloc(rgb_size);
    if (!rgb) {
      ESP_LOGE(TAG, "ai stream: RGB alloc failed");
      camera_frame_return(&frame);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    memset(rgb, 0, rgb_size);
    bool decoded = fmt2rgb888(frame.data, frame.size, PIXFORMAT_JPEG, rgb);
    camera_frame_return(&frame);

    if (!decoded) {
      ESP_LOGW(TAG, "ai stream: JPEG decode failed");
      free(rgb);
      continue;
    }

    /* Centre-crop and resize to 96×96 */
    uint8_t *cropped = crop_resize_rgb(rgb, src_w, src_h);
    free(rgb);
    if (!cropped) {
      ESP_LOGE(TAG, "ai stream: crop/resize alloc failed");
      continue;
    }

    /* Re-encode 96×96 RGB to JPEG */
    size_t jpg_len = 0;
    uint8_t *jpg =
        encode_rgb_to_jpeg(cropped, AI_VIEW_W, AI_VIEW_H, 80, &jpg_len);
    free(cropped);
    if (!jpg || jpg_len == 0) {
      ESP_LOGW(TAG, "ai stream: JPEG encode failed");
      if (jpg) {
        free(jpg);
      }
      continue;
    }

    /* Build MJPEG part header with X-Detection */
    const char *species = "unknown";
    switch (vr.kind) {
    case VISION_RESULT_CROW:
      species = "crow";
      break;
    case VISION_RESULT_MAGPIE:
      species = "magpie";
      break;
    case VISION_RESULT_SQUIRREL:
      species = "squirrel";
      break;
    case VISION_RESULT_BACKGROUND:
      species = "background";
      break;
    default:
      species = "unknown";
      break;
    }

    char detection_json[128];
    snprintf(detection_json, sizeof(detection_json),
             "{\"species\":\"%s\",\"confidence\":%.2f}", species,
             vr.confidence);

    int hdr_len = snprintf(part_hdr, sizeof(part_hdr), AI_PART_HDR_FMT,
                           (unsigned)jpg_len, detection_json);

    esp_err_t res = httpd_resp_send_chunk(req, part_hdr, hdr_len);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, PART_HDR_END, strlen(PART_HDR_END));
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)jpg, (int)jpg_len);
    }
    free(jpg);

    if (res != ESP_OK) {
      break;
    }
  }

  ESP_LOGI(TAG, "stream ai: client disconnected");
  cam_mgr_release();
  return ESP_OK;
}

/**
 * Main /stream handler — dispatches to live or AI based on ?mode= query param.
 */
static esp_err_t stream_handler(httpd_req_t *req) {
  char query[32] = {0};
  char mode[8] = "live"; /* default to live */

  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    httpd_query_key_value(query, "mode", mode, sizeof(mode));
  }

  if (strcmp(mode, "ai") == 0) {
    return stream_ai_handler(req);
  }
  return stream_live_handler(req);
}

/* ── GET /capture ────────────────────────────────────────────────────── */

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition",
                     "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, (int)fb->len);
  esp_camera_fb_return(fb);
  return res;
}

/* ── GET /status ─────────────────────────────────────────────────────── */

static esp_err_t status_handler(httpd_req_t *req) {
  max17048_reading_t fuel = {0};
  max17048_read(&fuel);

  int64_t uptime_us = esp_timer_get_time();
  int uptime_s = (int)(uptime_us / 1000000LL);

  char json[256];
  snprintf(json, sizeof(json),
           "{\"battery_v\":%.3f,\"battery_pct\":%.1f,\"uptime_s\":%d}",
           fuel.voltage_v, fuel.soc_percent, uptime_s);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, (int)strlen(json));
}

/* ── GET /events ─────────────────────────────────────────────────────── */

static esp_err_t events_handler(httpd_req_t *req) {
  snapshot_meta_t list[SNAPSHOT_MAX_COUNT];
  int count = 0;
  snapshot_list(list, &count);

  /* Build JSON array — newest first for the app */
  char *json = (char *)malloc(4096);
  if (!json) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  int pos = 0;
  pos += snprintf(json + pos, 4096 - pos, "[");
  for (int i = count - 1; i >= 0; --i) {
    const snapshot_meta_t *m = &list[i];
    int64_t ts_s = m->timestamp / 1000000LL;
    if (i < count - 1) {
      pos += snprintf(json + pos, 4096 - pos, ",");
    }
    pos += snprintf(json + pos, 4096 - pos,
                    "{\"id\":%d,\"species\":\"%s\",\"confidence\":%.2f,"
                    "\"timestamp\":%lld}",
                    m->id, m->species, m->confidence, (long long)ts_s);
  }
  pos += snprintf(json + pos, 4096 - pos, "]");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, json, pos);
  free(json);
  return res;
}

/* ── GET /events/<id>.jpg ────────────────────────────────────────────── */

static esp_err_t event_image_handler(httpd_req_t *req) {
  /* Parse snapshot ID from URI: /events/123.jpg */
  const char *uri = req->uri;
  const char *slash = strrchr(uri, '/');
  if (!slash) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  int id = atoi(slash + 1); /* atoi stops at '.jpg' */

  uint8_t *buf = NULL;
  size_t len = 0;
  esp_err_t err = snapshot_get(id, &buf, &len);
  if (err != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)buf, (int)len);
  free(buf);
  return res;
}

/* ── WebSocket handler ────────────────────────────────────────────────── */

static esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    /* New WebSocket connection — store the fd */
    int fd = httpd_req_to_sockfd(req);
    if (s_ws_count < WS_MAX_CLIENTS) {
      s_ws_fds[s_ws_count++] = fd;
      ESP_LOGI(TAG, "ws: client connected (fd=%d, total=%d)", fd, s_ws_count);
    } else {
      ESP_LOGW(TAG, "ws: max clients reached, rejecting fd=%d", fd);
    }
    return ESP_OK;
  }

  /* Handle incoming frames (we only expect close/ping) */
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "ws: recv failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Discard any incoming text — we are send-only */
  if (frame.len > 0) {
    uint8_t *buf = (uint8_t *)malloc(frame.len + 1);
    if (buf) {
      frame.payload = buf;
      httpd_ws_recv_frame(req, &frame, frame.len);
      free(buf);
    }
  }

  return ESP_OK;
}

/* Remove a disconnected fd from the tracking list */
static void ws_remove_fd(int fd) {
  for (int i = 0; i < s_ws_count; i++) {
    if (s_ws_fds[i] == fd) {
      s_ws_fds[i] = s_ws_fds[--s_ws_count];
      ESP_LOGI(TAG, "ws: client removed (fd=%d, remaining=%d)", fd, s_ws_count);
      return;
    }
  }
}

void http_server_notify_detection(const char *species, float confidence,
                                  int snap_id) {
  if (s_ws_count == 0 || !s_server) {
    return;
  }

  char json[192];
  int len =
      snprintf(json, sizeof(json),
               "{\"type\":\"detection\",\"species\":\"%s\",\"confidence\":%.2f,"
               "\"snap_id\":%d,\"timestamp\":%lld}",
               species, confidence, snap_id,
               (long long)(esp_timer_get_time() / 1000000));

  httpd_ws_frame_t frame = {
      .final = true,
      .fragmented = false,
      .type = HTTPD_WS_TYPE_TEXT,
      .payload = (uint8_t *)json,
      .len = (size_t)len,
  };

  /* Broadcast to all connected clients */
  for (int i = s_ws_count - 1; i >= 0; i--) {
    esp_err_t ret = httpd_ws_send_frame_async(s_server, s_ws_fds[i], &frame);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "ws: send failed fd=%d, removing", s_ws_fds[i]);
      ws_remove_fd(s_ws_fds[i]);
    }
  }

  ESP_LOGI(TAG, "ws: broadcast detection to %d clients", s_ws_count);
}

/* ── server start ────────────────────────────────────────────────────── */

esp_err_t http_server_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192;
  config.max_uri_handlers = 10;
  /* Allow long-running stream requests */
  config.lru_purge_enable = true;

  esp_err_t err = httpd_start(&s_server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }
  httpd_handle_t server = s_server;

  /* /stream */
  const httpd_uri_t uri_stream = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &uri_stream);

  /* /capture */
  const httpd_uri_t uri_capture = {
      .uri = "/capture",
      .method = HTTP_GET,
      .handler = capture_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &uri_capture);

  /* /status */
  const httpd_uri_t uri_status = {
      .uri = "/status",
      .method = HTTP_GET,
      .handler = status_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &uri_status);

  /* /events */
  const httpd_uri_t uri_events = {
      .uri = "/events",
      .method = HTTP_GET,
      .handler = events_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &uri_events);

  /* /events/<id>.jpg — wildcard for snapshot images */
  const httpd_uri_t uri_event_img = {
      .uri = "/events/*",
      .method = HTTP_GET,
      .handler = event_image_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &uri_event_img);

  /* /ws — WebSocket for real-time detection alerts */
  const httpd_uri_t uri_ws = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = ws_handler,
      .user_ctx = NULL,
      .is_websocket = true,
  };
  httpd_register_uri_handler(server, &uri_ws);

  ESP_LOGI(TAG, "HTTP server started on port %d (WS enabled)",
           config.server_port);
  return ESP_OK;
}
