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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "dali/common/helper.h"

namespace {

void ExpectEqual(unsigned long long actual, unsigned long long expected,
                 const std::string& test_name) {
  if (actual == expected) {
    return;
  }

  std::cerr << test_name << " failed: expected " << expected << ", got "
            << actual << "\n";
  std::exit(1);
}

unsigned long long CoverArea(std::vector<dali::RectI> rects) {
  return GetCoverArea(rects);
}

}  // namespace

int main() {
  ExpectEqual(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(0, 0, 10, 10)}),
              100, "cover_area_1");

  ExpectEqual(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 10, 10)}),
              100, "cover_area_2");

  ExpectEqual(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15)}),
              175, "cover_area_3");

  ExpectEqual(CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15),
                         dali::RectI(0, 5, 10, 15), dali::RectI(5, 0, 15, 10)}),
              225, "cover_area_4");

  ExpectEqual(
      CoverArea({dali::RectI(0, 0, 10, 10), dali::RectI(5, 5, 15, 15),
                 dali::RectI(20, 20, 30, 30), dali::RectI(20, 25, 30, 35)}),
      325, "cover_area_5");

  return 0;
}
