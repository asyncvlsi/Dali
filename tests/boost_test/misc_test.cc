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
#include <vector>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "dali/common/misc.h"
#define BOOST_TEST_MODULE misc

using namespace dali;

BOOST_AUTO_TEST_SUITE(misc)
BOOST_AUTO_TEST_CASE(cover_area_1) {
  RectI rect_0(0, 0, 10, 10);
  RectI rect_1(0, 0, 10, 10);
  std::set<RectI> rects;
  rects.insert(rect_0);
  rects.insert(rect_1);
  unsigned long long cover_area = GetCoverArea(rects);
  BOOST_CHECK_EQUAL(rects.size(), 1);
  BOOST_CHECK_EQUAL(cover_area, 100);
}

BOOST_AUTO_TEST_CASE(cover_area_2) {
  RectI rect_0(0, 0, 10, 10);
  RectI rect_1(5, 5, 10, 10);
  std::set<RectI> rects;
  rects.insert(rect_0);
  rects.insert(rect_1);
  unsigned long long cover_area = GetCoverArea(rects);
  BOOST_CHECK_EQUAL(rects.size(), 2);
  BOOST_CHECK_EQUAL(cover_area, 100);
}

BOOST_AUTO_TEST_CASE(cover_area_3) {
  RectI rect_0(0, 0, 10, 10);
  RectI rect_1(5, 5, 15, 15);
  std::set<RectI> rects;
  rects.insert(rect_0);
  rects.insert(rect_1);
  unsigned long long cover_area = GetCoverArea(rects);
  BOOST_CHECK_EQUAL(rects.size(), 2);
  BOOST_CHECK_EQUAL(cover_area, 175);
}

BOOST_AUTO_TEST_CASE(cover_area_4) {
  RectI rect_0(0, 0, 10, 10);
  RectI rect_1(5, 5, 15, 15);
  RectI rect_2(0, 5, 10, 15);
  RectI rect_3(5, 0, 15, 10);
  std::set<RectI> rects;
  rects.insert(rect_0);
  rects.insert(rect_1);
  rects.insert(rect_2);
  rects.insert(rect_3);
  unsigned long long cover_area = GetCoverArea(rects);
  BOOST_CHECK_EQUAL(rects.size(), 4);
  BOOST_CHECK_EQUAL(cover_area, 225);
}

BOOST_AUTO_TEST_SUITE_END()
