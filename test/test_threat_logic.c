#include "threat_logic.h"
#include "unity.h"


#define THRESHOLD 0.80f

/* --- is_threat tests --- */

static void test_crow_high_confidence_is_threat(void) {
  TEST_ASSERT_TRUE(is_threat(THREAT_VISION_CROW, 0.95f, THRESHOLD));
}

static void test_magpie_high_confidence_is_threat(void) {
  TEST_ASSERT_TRUE(is_threat(THREAT_VISION_MAGPIE, 0.90f, THRESHOLD));
}

static void test_squirrel_high_confidence_is_threat(void) {
  TEST_ASSERT_TRUE(is_threat(THREAT_VISION_SQUIRREL, 0.85f, THRESHOLD));
}

static void test_background_is_not_threat(void) {
  TEST_ASSERT_FALSE(is_threat(THREAT_VISION_BACKGROUND, 0.99f, THRESHOLD));
}

static void test_unknown_is_not_threat(void) {
  TEST_ASSERT_FALSE(is_threat(THREAT_VISION_UNKNOWN, 0.99f, THRESHOLD));
}

static void test_crow_low_confidence_is_not_threat(void) {
  TEST_ASSERT_FALSE(is_threat(THREAT_VISION_CROW, 0.50f, THRESHOLD));
}

static void test_exactly_at_threshold_is_threat(void) {
  TEST_ASSERT_TRUE(is_threat(THREAT_VISION_CROW, THRESHOLD, THRESHOLD));
}

static void test_just_below_threshold_is_not_threat(void) {
  TEST_ASSERT_FALSE(is_threat(THREAT_VISION_CROW, 0.79f, THRESHOLD));
}

/* --- get_vision_kind_str tests --- */

static void test_kind_str_crow(void) {
  TEST_ASSERT_EQUAL_STRING("CROW", get_vision_kind_str(THREAT_VISION_CROW));
}

static void test_kind_str_magpie(void) {
  TEST_ASSERT_EQUAL_STRING("MAGPIE", get_vision_kind_str(THREAT_VISION_MAGPIE));
}

static void test_kind_str_squirrel(void) {
  TEST_ASSERT_EQUAL_STRING("SQUIRREL",
                           get_vision_kind_str(THREAT_VISION_SQUIRREL));
}

static void test_kind_str_background(void) {
  TEST_ASSERT_EQUAL_STRING("BACKGROUND",
                           get_vision_kind_str(THREAT_VISION_BACKGROUND));
}

static void test_kind_str_unknown(void) {
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           get_vision_kind_str(THREAT_VISION_UNKNOWN));
}

static void test_kind_str_invalid(void) {
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           get_vision_kind_str((threat_vision_kind_t)99));
}

void test_threat_logic(void) {
  RUN_TEST(test_crow_high_confidence_is_threat);
  RUN_TEST(test_magpie_high_confidence_is_threat);
  RUN_TEST(test_squirrel_high_confidence_is_threat);
  RUN_TEST(test_background_is_not_threat);
  RUN_TEST(test_unknown_is_not_threat);
  RUN_TEST(test_crow_low_confidence_is_not_threat);
  RUN_TEST(test_exactly_at_threshold_is_threat);
  RUN_TEST(test_just_below_threshold_is_not_threat);
  RUN_TEST(test_kind_str_crow);
  RUN_TEST(test_kind_str_magpie);
  RUN_TEST(test_kind_str_squirrel);
  RUN_TEST(test_kind_str_background);
  RUN_TEST(test_kind_str_unknown);
  RUN_TEST(test_kind_str_invalid);
}
