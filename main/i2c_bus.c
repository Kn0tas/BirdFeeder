#include "i2c_bus.h"
#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";
static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t i2c_bus_init(void) {
  if (s_bus_handle != NULL) {
    return ESP_OK; // Already initialized
  }

  i2c_master_bus_config_t bus_config = {
      .i2c_port = I2C_PORT,
      .sda_io_num = PIN_I2C_SDA,
      .scl_io_num = PIN_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };

  esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "I2C bus initialized (SDA=%d SCL=%d)", PIN_I2C_SDA,
           PIN_I2C_SCL);
  return ESP_OK;
}

i2c_master_bus_handle_t get_i2c_bus_handle(void) { return s_bus_handle; }
