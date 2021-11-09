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
#include "misc.h"

#include "logging.h"

namespace dali {

unsigned long long GetCoverArea(
    std::set<RectI> &rects, int recursive_counter
) {
  DaliExpects(recursive_counter < 1000, "Something is wrong?");
  if (rects.empty()) {
    std::cout << recursive_counter << " " << 0 << "\n";
    return 0;
  }
  if (rects.size() == 1) {
    unsigned long long area = (unsigned long long) rects.begin()->Width() *
        (unsigned long long) rects.begin()->Height();
    std::cout << recursive_counter << " " << area << "\n";
    return area;
  }
  std::set<RectI> overlap_rects;
  unsigned long long sum_area = 0;
  for (auto it0 = rects.begin(); it0 != rects.end(); ++it0) {
    unsigned long long area = (unsigned long long) it0->Width() *
        (unsigned long long) it0->Height();
    sum_area += area;
    for (auto it1 = std::next(it0, 1); it1 != rects.end(); ++it1) {
      if (it0->IsOverlap(*it1)) {
        RectI overlap_rect = it0->GetOverlapRect(*it1);
        overlap_rects.insert(overlap_rect);
      }
    }
  }
  std::cout << recursive_counter << " " << sum_area << "\n";
  return sum_area - GetCoverArea(overlap_rects, recursive_counter + 1);
}

}
