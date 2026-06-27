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
#include "component_segment.h"

namespace dali {

void ComponentSegment::Merge(ComponentSegment& sc, int lower_bound,
                             int upper_bound) {
  int sz = (int)sc.component_ptrs.size();
  DaliExpects(sz == (int)sc.initial_loc.size(),
              "Component number does not match initial location number");
  for (int i = 0; i < sz; ++i) {
    component_ptrs.push_back(sc.component_ptrs[i]);
    initial_loc.push_back(sc.initial_loc[i]);
  }
  width_ += sc.Width();

  std::vector<double> anchor;
  int accumulative_width = 0;
  sz = (int)component_ptrs.size();
  for (int i = 0; i < sz; ++i) {
    anchor.push_back(initial_loc[i] - accumulative_width);
    accumulative_width += component_ptrs[i]->Width();
  }
  DaliExpects(width_ == accumulative_width,
              "Something is wrong, width does not match");

  double sum = 0;
  for (auto& num : anchor) {
    sum += num;
  }
  lx_ = (int)std::round(sum / sz);
  if (lx_ < lower_bound) {
    lx_ = lower_bound;
  }
  if (lx_ + width_ > upper_bound) {
    lx_ = upper_bound - width_;
  }
}

void ComponentSegment::UpdateComponentLocation() {
  int cur_loc = lx_;
  for (auto& component : component_ptrs) {
    component->SetLLX(cur_loc);
    cur_loc += component->Width();
  }
}

void ComponentSegment::Report() const {
  int sz = (int)component_ptrs.size();
  for (int i = 0; i < sz; ++i) {
    std::cout << component_ptrs[i]->Name() << "  " << component_ptrs[i]->LLX()
              << "  " << component_ptrs[i]->Width() << "  " << initial_loc[i]
              << "\n";
  }
}

}  // namespace dali
