/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "dali/common/misc.h"

#include <gtest/gtest.h>

#include <vector>

#include "dali/common/helper.h"

namespace {

unsigned long long CoverArea(std::vector<dali::RectI> rects) {
  return GetCoverArea(rects);
}

TEST(MiscTest, ComputesCoverAreaForIdenticalRectangles) {
  EXPECT_EQ(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(0, 0, 10, 10)}),
            100);
}

TEST(MiscTest, ComputesCoverAreaForContainedRectangles) {
  EXPECT_EQ(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 10, 10)}),
            100);
}

TEST(MiscTest, ComputesCoverAreaForOverlappingRectangles) {
  EXPECT_EQ(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15)}),
            175);
}

TEST(MiscTest, ComputesCoverAreaForFourOverlappingRectangles) {
  EXPECT_EQ(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15),
                       dali::RectI(0, 5, 10, 15), dali::RectI(5, 0, 15, 10)}),
            225);
}

TEST(MiscTest, ComputesCoverAreaForDisjointAndOverlappingRectangles) {
  EXPECT_EQ(
      CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15),
                 dali::RectI(20, 20, 30, 30), dali::RectI(20, 25, 30, 35)}),
      325);
}

}  // namespace
