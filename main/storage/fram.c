#include "fram.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "fram";

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
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "fram init stub");
    return ESP_OK;
}

esp_err_t fram_write_config(const void *data, size_t len) {
    (void)data;
    (void)len;
    // TODO: implement FRAM write sequence
    return ESP_OK;
}

esp_err_t fram_read_config(void *data, size_t len) {
    (void)data;
    (void)len;
    // TODO: implement FRAM read sequence
    return ESP_OK;
}
