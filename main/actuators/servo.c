#include "servo.h"
#include "board_config.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "servo_math.h"

static const char *TAG = "servo";
static bool s_inited = false;

esp_err_t servo_init(void) {
  ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = SERVO_PWM_RES_BITS,
      .timer_num =
          LEDC_TIMER_1, // TIMER_0 is used by camera XCLK — must not conflict
      .freq_hz = SERVO_PWM_FREQ_HZ,
      .clk_cfg = LEDC_USE_APB_CLK,
  };
  esp_err_t err = ledc_timer_config(&timer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "timer config failed: %s", esp_err_to_name(err));
    return err;
  }

  ledc_channel_config_t ch = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_1, // CHANNEL_0 is used by camera XCLK
      .timer_sel = LEDC_TIMER_1,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = PIN_SERVO,
      .duty = servo_pulse_to_duty(clamp_pulse(SERVO_PULSE_OPEN_US)),
      .hpoint = 0,
  };
  err = ledc_channel_config(&ch);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "channel config failed: %s", esp_err_to_name(err));
    return err;
  }

  s_inited = true;
  ESP_LOGI(TAG, "servo init done on GPIO%d", PIN_SERVO);
  return ESP_OK;
}

esp_err_t servo_set_lid_closed(bool closed) {
  if (!s_inited) {
    return ESP_ERR_INVALID_STATE;
  }
  uint32_t pulse = closed ? SERVO_PULSE_CLOSED_US : SERVO_PULSE_OPEN_US;
  pulse = clamp_pulse(pulse);
  uint32_t duty = servo_pulse_to_duty(pulse);
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "set_duty failed: %s", esp_err_to_name(err));
    return err;
  }
  err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "update_duty failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "servo moved to %s (pulse %uus)", closed ? "closed" : "open",
           pulse);
  return ESP_OK;
}
