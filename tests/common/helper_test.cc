/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#include "dali/common/helper.h"

#include <gtest/gtest.h>

TEST(HelperTest, ParsesWidthByHeight) {
  int width = 0;
  int height = 0;
  ASSERT_TRUE(dali::ParseWidthByHeight("1400x900", &width, &height));
  EXPECT_EQ(width, 1400);
  EXPECT_EQ(height, 900);
}

TEST(HelperTest, RejectsMalformedWidthByHeightAndLeavesOutputs) {
  for (const char *text : {"1400", "x900", "1400x", "0x900", "1400x0",
                           "1400x900px", "-5x9", "14 00x900", "1400X900",
                           "99999999x1", ""}) {
    int width = 7;
    int height = 8;
    EXPECT_FALSE(dali::ParseWidthByHeight(text, &width, &height)) << text;
    EXPECT_EQ(width, 7) << text;
    EXPECT_EQ(height, 8) << text;
  }
}
