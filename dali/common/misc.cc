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

#include <unordered_set>
#include <unordered_map>

namespace dali {

class SegmentTree {
 public:
  int start, end;
  std::vector<int> X;
  SegmentTree *left;
  SegmentTree *right;
  int count;
  long long total;

  SegmentTree(int start0, int end0, std::vector<int> &X0) :
      start(start0),
      end(end0),
      X(X0) {
    left = nullptr;
    right = nullptr;
    count = 0;
    total = 0;
  }

  int GetRangeMid() const {
    return start + (end - start) / 2;
  }

  SegmentTree *GetLeft() {
    if (left == nullptr) {
      left = new SegmentTree(start, GetRangeMid(), X);
    }
    return left;
  }

  SegmentTree *GetRight() {
    if (right == nullptr) {
      right = new SegmentTree(GetRangeMid(), end, X);
    }
    return right;
  }

  long long Update(int i, int j, int val);
};

long long SegmentTree::Update(int i, int j, int val) {
  if (i >= j) return 0;
  if (start == i && end == j) {
    count += val;
  } else {
    GetLeft()->Update(i, std::min(GetRangeMid(), j), val);
    GetRight()->Update(std::max(GetRangeMid(), i), j, val);
  }

  if (count > 0) total = X[end] - X[start];
  else total = GetLeft()->total + GetRight()->total;

  return total;
}

unsigned long long GetCoverArea(std::vector<RectI> &rects) {
  // no rectangles, return 0 immediately
  if (rects.empty()) return 0;

  int event_open = 1, event_close = -1;
  // event is a tuple containing {y_loc, open/close event, lower_x, upper_x}
  std::vector<std::vector<int>> events;
  // each rectangle can create at most 2 events if area is positive, otherwise 0 event
  events.reserve(rects.size() * 2);
  std::unordered_set<int> x_values_set;

  // create an event only when a rectangle has an area larger than 0
  for (auto &rec: rects) {
    if (!rec.CheckValidity()) {
      DaliExpects(false,
                  "Something wrong with this rectangle? " + rec.ToString());
    }
    if ((rec.LLX() == rec.URX()) || (rec.LLY() == rec.URY())) continue;
    events.push_back({rec.LLY(), event_open, rec.LLX(), rec.URX()});
    events.push_back({rec.URY(), event_close, rec.LLX(), rec.URX()});
    x_values_set.insert(rec.LLX());
    x_values_set.insert(rec.URX());
  }

  // if there is no events, return 0 earlier
  if (events.empty()) return 0;

  // sort all events based on y location of an event
  std::sort(
      events.begin(),
      events.end(),
      [](std::vector<int> const &event0, std::vector<int> const &event1) {
        return event0[0] < event1[0];
      }
  );

  // build a map to find index from x value
  std::vector<int> x_values;
  int sz = static_cast<int>(x_values_set.size());
  x_values.reserve(sz);
  for (auto &val: x_values_set) {
    x_values.push_back(val);
  }
  std::sort(x_values.begin(), x_values.end());
  std::unordered_map<int, int> x_value_id_map;
  for (int i = 0; i < sz; ++i) {
    x_value_id_map.insert(
        std::unordered_map<int, int>::value_type(x_values[i], i)
    );
  }

  SegmentTree active(0, sz - 1, x_values);
  long long ans = 0;
  long long cur_x_sum = 0;
  int cur_y = events[0][0];
  for (auto &event: events) {
    int y = event[0], type = event[1], x1 = event[2], x2 = event[3];
    ans += cur_x_sum * (y - cur_y);
    cur_x_sum = active.Update(x_value_id_map[x1], x_value_id_map[x2], type);
    cur_y = y;
  }

  return ans;
}

}
