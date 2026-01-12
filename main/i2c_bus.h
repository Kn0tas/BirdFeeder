#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared I2C master bus (Port 0)
 * @return ESP_OK on success
 */
esp_err_t i2c_bus_init(void);

/**
 * @brief Get the shared I2C master bus handle
 * @return Handle or NULL if not initialized
 */
i2c_master_bus_handle_t get_i2c_bus_handle(void);

#ifdef __cplusplus
}
#endif
