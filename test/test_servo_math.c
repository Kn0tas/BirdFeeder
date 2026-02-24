#include "servo_math.h"
#include "unity.h"


static void test_clamp_pulse_within_range(void) {
  TEST_ASSERT_EQUAL_UINT32(1500, clamp_pulse(1500));
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, clamp_pulse(SERVO_PULSE_MIN_US));
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US, clamp_pulse(SERVO_PULSE_MAX_US));
}

static void test_clamp_pulse_below_min(void) {
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, clamp_pulse(0));
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US, clamp_pulse(100));
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MIN_US,
                           clamp_pulse(SERVO_PULSE_MIN_US - 1));
}

static void test_clamp_pulse_above_max(void) {
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US, clamp_pulse(5000));
  TEST_ASSERT_EQUAL_UINT32(SERVO_PULSE_MAX_US,
                           clamp_pulse(SERVO_PULSE_MAX_US + 1));
}

static void test_pulse_to_duty_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(0, servo_pulse_to_duty(0));
}

static void test_pulse_to_duty_known_value(void) {
  /* For 50Hz, 14-bit (max_duty = 16383):
     duty = pulse_us * 50 * 16383 / 1_000_000
     For 1500us: 1500 * 50 * 16383 / 1_000_000 = 1228 (truncated) */
  uint32_t duty = servo_pulse_to_duty(1500);
  TEST_ASSERT_EQUAL_UINT32(1228, duty);
}

static void test_pulse_to_duty_min_max(void) {
  /* 500us → 500 * 50 * 16383 / 1_000_000 = 409 */
  TEST_ASSERT_EQUAL_UINT32(409, servo_pulse_to_duty(SERVO_PULSE_MIN_US));
  /* 2500us → 2500 * 50 * 16383 / 1_000_000 = 2047 */
  TEST_ASSERT_EQUAL_UINT32(2047, servo_pulse_to_duty(SERVO_PULSE_MAX_US));
}

void test_servo_math(void) {
  RUN_TEST(test_clamp_pulse_within_range);
  RUN_TEST(test_clamp_pulse_below_min);
  RUN_TEST(test_clamp_pulse_above_max);
  RUN_TEST(test_pulse_to_duty_zero);
  RUN_TEST(test_pulse_to_duty_known_value);
  RUN_TEST(test_pulse_to_duty_min_max);
}
