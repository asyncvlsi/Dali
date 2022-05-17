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

struct BlkDispVarSegment {
 private:
  double lx_;
  int width_;
  double sum_es_;
  double sum_e_;
 public:
  BlkDispVarSegment(BlkDispVar *var, double lx) :
      lx_(lx),
      width_(var->Width()) {
    vars_.push_back(var);
    sum_es_ = var->Weight() * var->InitX();
    sum_e_ = var->Weight();
  }
  std::vector<BlkDispVar *> vars_;

  double LX() const { return lx_; }
  double UX() const { return lx_ + width_; }
  int Width() const { return width_; }

  bool IsNotOnLeft(BlkDispVarSegment &sc) const {
    return sc.LX() < UX();
  }
  void Merge(
      BlkDispVarSegment &seg,
      double lower_bound,
      double upper_bound
  );
  void LinearMerge(
      BlkDispVarSegment &seg,
      double lower_bound,
      double upper_bound
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
    double lower_bound,
    double upper_bound
) {
  vars_.reserve(vars_.size() + seg.vars_.size());
  for (auto &var: seg.vars_) {
    sum_es_ += var->Weight() * (var->InitX() - width_);
    sum_e_ += var->Weight();
    width_ += var->Width();
    vars_.push_back(var);
  }

  lx_ = sum_es_ / sum_e_;
  if (lx_ < lower_bound) {
    lx_ = lower_bound;
  }
  if (lx_ + width_ > upper_bound) {
    lx_ = upper_bound - width_;
  }
}

void BlkDispVarSegment::LinearMerge(
    BlkDispVarSegment &seg,
    double lower_bound,
    double upper_bound
) {
  vars_.reserve(vars_.size() + seg.vars_.size());
  for (auto &var: seg.vars_) {
    vars_.push_back(var);
  }
  sum_e_ += seg.sum_e_;
  width_ += seg.width_;

  struct WeightLocPair {
    WeightLocPair(double init_e, double init_s) : e(init_e), s(init_s) {}
    double e;
    double s;
  };

  std::vector<WeightLocPair> es_;
  es_.reserve(vars_.size());
  int accumulative_width = 0;
  for (auto &var: vars_) {
    es_.emplace_back(var->Weight(), var->InitX() - accumulative_width);
    accumulative_width += var->Width();
  }

  std::sort(
      es_.begin(),
      es_.end(),
      [](const WeightLocPair &pair0, const WeightLocPair &pair1) {
        return pair0.s < pair1.s;
      }
  );

  double accumulative_weight = 0;
  for (auto &[e, s]: es_) {
    accumulative_weight += e;
    if (2 * accumulative_weight >= sum_e_) {
      lx_ = s;
      break;
    }
  }

  if (lx_ < lower_bound) {
    lx_ = lower_bound;
  }
  if (lx_ + width_ > upper_bound) {
    lx_ = upper_bound - width_;
  }
}

void BlkDispVarSegment::UpdateVarLoc() {
  double cur_loc = lx_;
  for (auto &var: vars_) {
    var->SetSolution(cur_loc);
    var->SetClusterWeight(sum_e_);
    cur_loc += var->Width();
  }
}

/****
 * @brief This function is for optimizing displacement.
 * Assuming the objective function is like this:
 *     obj = sum_i ( e_i(x_i - x_i0)^2 + a_i(x_i - x_ia)^2 )
 * where x_i is the location of cell i, x_i0 is its initial location,
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
    double lower_limit,
    double upper_limit
) {
  std::vector<BlkDispVarSegment> segments;

  size_t sz = vars.size();
  for (size_t i = 0; i < sz; ++i) {
    // create a new segment which contains only this block
    double lx = vars[i].x_0;
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

/****
 * @brief This function is for optimizing displacement.
 * Assuming the objective function is like this:
 *     obj = sum_i e_i|x_i - x_i0|
 * where x_i is the location of cell i, x_i0 is its initial location,
 * e_i is the weight, which is always non-negative, and most often 1
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
void MinimizeLinearDisplacement(
    std::vector<BlkDispVar> &vars,
    double lower_limit,
    double upper_limit
) {
  std::vector<BlkDispVarSegment> segments;

  size_t sz = vars.size();
  for (size_t i = 0; i < sz; ++i) {
    // create a new segment which contains only this block
    double lx = vars[i].x_0;
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
      prev_seg->LinearMerge(*cur_seg, lower_limit, upper_limit);
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

struct blocksegment {
  int first_id = -1;
  int last_id = -1;
  double x = 0;
  double sum_e_ = 0;
  double sum_es_ = 0;
  int width = 0;

  int CellCount() const { return last_id - first_id + 1; }
  void UpdatePosition() { x = sum_es_ / sum_e_; }
  void AddCell(BlkDispVar &var, int i);
  void SetX(double init_x) { x = init_x; }
  void SetFirstId(int i) { first_id = i; }
  int LastId() { return last_id; }
  int Width() { return width; }
  double TotalWeight() { return sum_e_; }
  double TotalWeightedLoc() { return sum_es_; }
  double LX() const { return x; }
  double UX() const { return x + width; }
  void AddSegment(blocksegment &seg);
};

void blocksegment::AddCell(BlkDispVar &var, int i) {
  last_id = i;
  sum_e_ += var.Weight();
  sum_es_ += var.Weight() * (var.InitX() - width);
  width += var.Width();
}

void blocksegment::AddSegment(blocksegment &seg) {
  last_id = seg.LastId();
  sum_e_ += seg.TotalWeight();
  sum_es_ += seg.TotalWeightedLoc() - seg.TotalWeight() * width;
  width += seg.Width();
}

void CollapseSegment(
    std::vector<blocksegment> &segments,
    double lower_limit,
    double upper_limit
) {
  DaliExpects(!segments.empty(), "Impossible to be empty!");
  blocksegment &cur_seg = segments.back();
  cur_seg.UpdatePosition();
  if (cur_seg.LX() < lower_limit) {
    cur_seg.SetX(lower_limit);
  }
  if (cur_seg.UX() > upper_limit) {
    cur_seg.SetX(upper_limit - cur_seg.Width());
  }

  size_t seg_sz = segments.size();
  if (seg_sz == 1) return;
  blocksegment &prev_seg = segments[seg_sz - 2];
  if (prev_seg.UX() > cur_seg.LX()) {
    prev_seg.AddSegment(cur_seg);
    segments.pop_back();
    CollapseSegment(segments, lower_limit, upper_limit);
  }
}

void AbacusPlaceRow(
    std::vector<BlkDispVar> &vars,
    double lower_limit,
    double upper_limit
) {
  if (vars.empty()) return;
  std::vector<blocksegment> segments;

  int sz = static_cast<int>(vars.size());
  for (int i = 0; i < sz; ++i) {
    if ((i == 0) || (segments.back().UX() <= vars[i].InitX())) {
      segments.emplace_back();
      blocksegment &last_seg = segments.back();
      last_seg.SetX(vars[i].InitX());
      last_seg.SetFirstId(i);
      last_seg.AddCell(vars[i], i);
    } else {
      blocksegment &last_seg = segments.back();
      last_seg.AddCell(vars[i], i);
      CollapseSegment(segments, lower_limit, upper_limit);
    }
  }

  int i = 0;
  for (auto &seg: segments) {
    double x = seg.LX();
    for (; i <= seg.LastId(); ++i) {
      vars[i].SetSolution(x);
      vars[i].SetClusterWeight(seg.TotalWeight());
      x += vars[i].Width();
    }
  }
}

}
