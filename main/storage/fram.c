#include "fram.h"
#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include <stdlib.h>
#include <string.h>


static const char *TAG = "fram";
static const uint16_t FRAM_CAP_BYTES = 32768; // 256 Kbit device
static i2c_master_dev_handle_t s_fram_handle = NULL;

static esp_err_t fram_write_bytes(uint16_t addr, const uint8_t *data,
                                  size_t len) {
  if (!data || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if ((addr + len) > FRAM_CAP_BYTES) {
    return ESP_ERR_INVALID_SIZE;
  }
  if (!s_fram_handle) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t *buf = (uint8_t *)malloc(len + 2);
  if (!buf) {
    return ESP_ERR_NO_MEM;
  }
  buf[0] = (uint8_t)(addr >> 8);
  buf[1] = (uint8_t)(addr & 0xFF);
  memcpy(buf + 2, data, len);

  esp_err_t err = i2c_master_transmit(s_fram_handle, buf, len + 2, -1);
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
  if (!s_fram_handle) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t addr_bytes[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
  esp_err_t err = i2c_master_transmit_receive(
      s_fram_handle, addr_bytes, sizeof(addr_bytes), data, len, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
  }
  return err;
}

esp_err_t fram_init(void) {
  i2c_master_bus_handle_t bus_handle = get_i2c_bus_handle();
  if (!bus_handle) {
    ESP_LOGE(TAG, "I2C bus not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = FRAM_I2C_ADDR,
      .scl_speed_hz = I2C_CLK_HZ,
  };

  esp_err_t err =
      i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_fram_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "failed to add device: %s", esp_err_to_name(err));
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
