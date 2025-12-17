#include "fram.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "fram";
static const uint16_t FRAM_CAP_BYTES = 32768; // 256 Kbit device

static esp_err_t fram_write_bytes(uint16_t addr, const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((addr + len) > FRAM_CAP_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *buf = (uint8_t *)malloc(len + 2);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(buf + 2, data, len);

    esp_err_t err = i2c_master_write_to_device(I2C_PORT, FRAM_I2C_ADDR, buf, len + 2, pdMS_TO_TICKS(100));
    free(buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t fram_read_bytes(uint16_t addr, uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((addr + len) > FRAM_CAP_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t addr_bytes[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    esp_err_t err = i2c_master_write_read_device(I2C_PORT, FRAM_I2C_ADDR, addr_bytes, sizeof(addr_bytes), data, len, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t fram_init(void) {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLK_HZ,
        .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(I2C_PORT, &cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "fram ready (addr 0x%02X)", FRAM_I2C_ADDR);
    return ESP_OK;
}

esp_err_t fram_write_config(const void *data, size_t len) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    return fram_write_bytes(0, (const uint8_t *)data, len);
}

esp_err_t fram_read_config(void *data, size_t len) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    return fram_read_bytes(0, (uint8_t *)data, len);
}
