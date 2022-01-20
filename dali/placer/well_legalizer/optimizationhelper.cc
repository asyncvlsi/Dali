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
#include "optimizationhelper.h"

#include "dali/common/helper.h"
#include "dali/common/logging.h"

namespace dali {

/****
 * @brief return the weight for optimal anchor
 *
 * Important: must return 0 when i is 0
 *
 * @param i
 * @return
 */
double GetOptimalAnchorWeight(int i) {
  //return i * 0.1;
  return i * i * 0.01;
  //return exp(i/10.0) - 1;
}

struct BlkDispVarSegment {
 private:
  int lx_;
  int width_;
  double sum_es_;
  double sum_e_;
 public:
  BlkDispVarSegment(BlkDispVar *var, int lx) :
      lx_(lx),
      width_(var->Width()) {
    vars_.push_back(var);
    sum_es_ = var->Weight() * var->InitX();
    sum_e_ = var->Weight();
  }
  std::vector<BlkDispVar *> vars_;

  int LX() const { return lx_; }
  int UX() const { return lx_ + width_; }
  int Width() const { return width_; }

  bool IsNotOnLeft(BlkDispVarSegment &sc) const {
    return sc.LX() < UX();
  }
  void Merge(
      BlkDispVarSegment &seg,
      int lower_bound = INT_MIN,
      int upper_bound = INT_MAX
  );
  void UpdateVarLoc();
};

/****
 * @brief compute the optimal x location
 * why this code works can be found in my thesis
 *
 * @param seg
 * @param lower_bound
 * @param upper_bound
 */
void BlkDispVarSegment::Merge(
    BlkDispVarSegment &seg,
    int lower_bound,
    int upper_bound
) {
  vars_.reserve(vars_.size() + seg.vars_.size());
  for (auto &var: seg.vars_) {
    sum_es_ += var->Weight() * (var->InitX() - width_);
    sum_e_ += var->Weight();
    width_ += var->Width();
    vars_.push_back(var);
  }

  lx_ = (int) std::round(sum_es_ / sum_e_);
  if (lx_ < lower_bound) {
    lx_ = lower_bound;
  }
  if (lx_ + width_ > upper_bound) {
    lx_ = upper_bound - width_;
  }
}

void BlkDispVarSegment::UpdateVarLoc() {
  int cur_loc = lx_;
  for (auto &var: vars_) {
    var->SetSolution(cur_loc);
    var->SetClusterWeight(vars_.size());
    cur_loc += var->Width();
  }
}

/****
 * @brief This function is for optimizing displacement.
 * Assuming the objective function is like this:
 *     obj = sum_i ( e_i(x_i - x_i0)^2 + a_i(x_i - x_ia)^2 )
 * where x_i is the final location, x_i0 is the initial location,
 * x_ia is the anchor location if there is one, e_i is usually 1,
 * and a_i is 0 if there is no anchor,
 * the final solution needs to satisfy the following constraints:
 *     lower_bound <= x_0
 *     x_0 + width_0 <= x_1
 *     x_1 + width_1 <= x_2
 *     ...
 *     x_(n-2) + width_(n-2) <= x_(n-1)
 *     x_(n-1) + width_(n-1) <= upper_bound
 *
 * @param vars: each of this contains x_i, x_i0, e_i, x_ia, a_i, and width_i
 * @param lower_limit: lower bound
 * @param upper_limit: upper bound
 */
void MinimizeQuadraticDisplacement(
    std::vector<BlkDispVar> &vars,
    int lower_limit,
    int upper_limit
) {
  std::vector<BlkDispVarSegment> segments;

  size_t sz = vars.size();
  for (size_t i = 0; i < sz; ++i) {
    // create a new segment which contains only this block
    int lx = static_cast<int>(std::round(vars[i].x_0));
    lx = std::max(lx, lower_limit);
    lx = std::min(lx, upper_limit - vars[i].w);
    segments.emplace_back(&vars[i], lx);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left
    BlkDispVarSegment *cur_seg = &(segments[seg_sz - 1]);
    BlkDispVarSegment *prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnLeft(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_limit, upper_limit);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  for (auto &seg: segments) {
    seg.UpdateVarLoc();
  }
}

}
