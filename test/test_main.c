#include "unity.h"

/* Forward-declare all test suite runners */
extern void test_servo_math(void);
extern void test_vision_utils(void);
extern void test_threat_logic(void);
extern void test_sensor_conversions(void);
extern void test_fram_bounds(void);

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();

  test_servo_math();
  test_vision_utils();
  test_threat_logic();
  test_sensor_conversions();
  test_fram_bounds();

  return UNITY_END();
}
