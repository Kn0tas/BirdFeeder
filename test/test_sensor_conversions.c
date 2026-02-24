#include "max17048_math.h"
#include "unity.h"
#include <math.h>


/* Helper: compare floats with tolerance */
#define ASSERT_FLOAT_NEAR(expected, actual, tol)                               \
  TEST_ASSERT_FLOAT_WITHIN((tol), (expected), (actual))

/* --- Voltage conversion tests --- */

static void test_voltage_zero(void) {
  /* VCELL = 0x0000 → vraw = 0 → 0V */
  ASSERT_FLOAT_NEAR(0.0f, max17048_raw_to_voltage(0x00, 0x00), 0.001f);
}

static void test_voltage_known_value(void) {
  /* Example: 4.2V battery
     vraw = 4200 / 1.25 * 1000 = 3360
     3360 << 4 = 53760 = 0xD200
     msb=0xD2, lsb=0x00
     Reverse: vraw = 0xD200 >> 4 = 0xD20 = 3360
     voltage = 3360 * 1.25 / 1000 = 4.2V */
  ASSERT_FLOAT_NEAR(4.2f, max17048_raw_to_voltage(0xD2, 0x00), 0.01f);
}

static void test_voltage_3v7(void) {
  /* 3.7V → vraw = 2960, 2960 << 4 = 47360 = 0xB900
     msb=0xB9, lsb=0x00 */
  ASSERT_FLOAT_NEAR(3.7f, max17048_raw_to_voltage(0xB9, 0x00), 0.01f);
}

/* --- SOC conversion tests --- */

static void test_soc_full(void) {
  /* 100% → msb=100, lsb=0 */
  ASSERT_FLOAT_NEAR(100.0f, max17048_raw_to_soc(100, 0), 0.01f);
}

static void test_soc_zero(void) {
  ASSERT_FLOAT_NEAR(0.0f, max17048_raw_to_soc(0, 0), 0.01f);
}

static void test_soc_fractional(void) {
  /* 75.5% → msb=75, lsb=128 (128/256 = 0.5) */
  ASSERT_FLOAT_NEAR(75.5f, max17048_raw_to_soc(75, 128), 0.01f);
}

static void test_soc_quarter(void) {
  /* 50.25% → msb=50, lsb=64 (64/256 = 0.25) */
  ASSERT_FLOAT_NEAR(50.25f, max17048_raw_to_soc(50, 64), 0.01f);
}

void test_sensor_conversions(void) {
  RUN_TEST(test_voltage_zero);
  RUN_TEST(test_voltage_known_value);
  RUN_TEST(test_voltage_3v7);
  RUN_TEST(test_soc_full);
  RUN_TEST(test_soc_zero);
  RUN_TEST(test_soc_fractional);
  RUN_TEST(test_soc_quarter);
}
