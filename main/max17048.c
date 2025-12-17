#include "max17048.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "max17048";

#define REG_VCELL   0x02
#define REG_SOC     0x04
#define REG_MODE    0x06
#define REG_VERSION 0x08

static esp_err_t max17048_read_reg(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_PORT, MAX17048_I2C_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t max17048_write_reg16(uint8_t reg, uint16_t value) {
    uint8_t buf[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return i2c_master_write_to_device(I2C_PORT, MAX17048_I2C_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

esp_err_t max17048_quickstart(void) {
    esp_err_t err = max17048_write_reg16(REG_MODE, 0x4000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "quickstart failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t max17048_init(void) {
    uint8_t ver[2] = {0};
    esp_err_t err = max17048_read_reg(REG_VERSION, ver, sizeof(ver));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "version read failed: %s", esp_err_to_name(err));
        return err;
    }
    uint16_t version = ((uint16_t)ver[0] << 8) | ver[1];
    ESP_LOGI(TAG, "MAX17048 detected (version 0x%04X)", version);
    return ESP_OK;
}

esp_err_t max17048_read(max17048_reading_t *out) {
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[2];

    esp_err_t err = max17048_read_reg(REG_VCELL, buf, sizeof(buf));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "vcell read failed: %s", esp_err_to_name(err));
        return err;
    }
    uint16_t vraw = ((uint16_t)buf[0] << 8) | buf[1];
    vraw >>= 4;
    out->voltage_v = (float)vraw * 1.25f / 1000.0f;

    err = max17048_read_reg(REG_SOC, buf, sizeof(buf));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "soc read failed: %s", esp_err_to_name(err));
        return err;
    }
    out->soc_percent = (float)buf[0] + ((float)buf[1] / 256.0f);

    return ESP_OK;
}
