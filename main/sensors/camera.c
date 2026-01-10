#include "camera.h"
#include "board_config.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "camera";
static bool s_inited = false;

static void camera_power_and_reset(void) {
    if (PIN_CAM_PWDN >= 0) {
        gpio_reset_pin(PIN_CAM_PWDN);
        gpio_set_direction(PIN_CAM_PWDN, GPIO_MODE_OUTPUT);
        // OV2640 PWDN high = power-down; drive low to enable
        gpio_set_level(PIN_CAM_PWDN, 0);
    }
    if (PIN_CAM_RESET >= 0) {
        gpio_reset_pin(PIN_CAM_RESET);
        gpio_set_direction(PIN_CAM_RESET, GPIO_MODE_OUTPUT);
        // Reset pulse: low -> high
        gpio_set_level(PIN_CAM_RESET, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(PIN_CAM_RESET, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t camera_init(void) {
    camera_power_and_reset();

    camera_config_t cfg = {
        .pin_pwdn = PIN_CAM_PWDN,
        .pin_reset = PIN_CAM_RESET,
        .pin_xclk = PIN_CAM_XCLK,
        .pin_sscb_sda = PIN_CAM_SIOD,
        .pin_sscb_scl = PIN_CAM_SIOC,
        .sccb_i2c_port = 1,          // use I2C1 for SCCB (pins 38/39)

        // Breakout labels D2-D9 map to esp_camera D0-D7 in order
        .pin_d7 = PIN_CAM_D9,
        .pin_d6 = PIN_CAM_D8,
        .pin_d5 = PIN_CAM_D7,
        .pin_d4 = PIN_CAM_D6,
        .pin_d3 = PIN_CAM_D5,
        .pin_d2 = PIN_CAM_D4,
        .pin_d1 = PIN_CAM_D3,
        .pin_d0 = PIN_CAM_D2,
        .pin_vsync = PIN_CAM_VSYNC,
        .pin_href = PIN_CAM_HREF,
        .pin_pclk = PIN_CAM_PCLK,

        .xclk_freq_hz = 20000000,    // OV2640 often prefers 20 MHz
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QQVGA, // smaller to fit DRAM if PSRAM fails
        .jpeg_quality = 20,            // lighter compression workload
        .fb_count = 2,                 // double buffer to reduce stale frames
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_LATEST, // drop old frames after idle
    };

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }
    s_inited = true;
    // Flush a couple of frames after init to avoid stale/invalid JPEGs.
    for (int i = 0; i < 2; ++i) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    ESP_LOGI(TAG, "camera ready (QQVGA/JPEG)");
    return ESP_OK;
}

esp_err_t camera_capture(camera_frame_t *out_frame) {
    if (!out_frame || !s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        return ESP_FAIL;
    }
    out_frame->fb_handle = fb;
    out_frame->data = fb->buf;
    out_frame->size = fb->len;
    out_frame->width = fb->width;
    out_frame->height = fb->height;
    out_frame->is_jpeg = fb->format == PIXFORMAT_JPEG;
    return ESP_OK;
}

void camera_frame_return(camera_frame_t *frame) {
    if (frame && frame->fb_handle) {
        esp_camera_fb_return((camera_fb_t *)frame->fb_handle);
        frame->fb_handle = NULL;
    }
}

void camera_dump_base64(const camera_frame_t *frame) {
    if (!frame || !frame->data || frame->size == 0) {
        ESP_LOGW(TAG, "no frame to dump");
        return;
    }
    size_t out_len = 4 * ((frame->size + 2) / 3) + 1;
    uint8_t *out = (uint8_t *)malloc(out_len);
    if (!out) {
        ESP_LOGE(TAG, "base64 alloc failed (%u bytes)", (unsigned)out_len);
        return;
    }
    size_t written = 0;
    int rc = mbedtls_base64_encode(out, out_len, &written, frame->data, frame->size);
    if (rc != 0) {
        ESP_LOGE(TAG, "base64 encode failed (%d)", rc);
        free(out);
        return;
    }

    printf("\n---FRAME-START %u %d %d---\n", (unsigned)frame->size, frame->width, frame->height);
    const size_t chunk = 120;
    for (size_t i = 0; i < written; i += chunk) {
        size_t n = written - i;
        if (n > chunk) n = chunk;
        fwrite(out + i, 1, n, stdout);
        printf("\n");
    }
    printf("---FRAME-END---\n");
    fflush(stdout);
    free(out);
}
