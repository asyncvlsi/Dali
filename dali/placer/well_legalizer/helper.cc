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
#include "helper.h"

#include "dali/common/helper.h"
#include "dali/common/logging.h"

namespace dali {

void GenClusterTable(
    std::string const &name_of_file,
    std::vector<ClusterStripe> &col_list_
) {
  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.gridded_rows_) {
        std::vector<int> llx;
        std::vector<int> lly;
        std::vector<int> urx;
        std::vector<int> ury;

        llx.push_back(cluster.LLX());
        lly.push_back(cluster.LLY());
        urx.push_back(cluster.URX());
        ury.push_back(cluster.URY());

        size_t sz = llx.size();
        for (size_t i = 0; i < sz; ++i) {
          ost << llx[i] << "\t"
              << urx[i] << "\t"
              << urx[i] << "\t"
              << llx[i] << "\t"
              << lly[i] << "\t"
              << lly[i] << "\t"
              << ury[i] << "\t"
              << ury[i] << "\n";
        }
      }
    }
  }
  ost.close();
}

void CollectWellFillingRects(
    Stripe &stripe,
    int bottom_boundary,
    int top_boundary,
    std::vector<RectI> &n_rects, std::vector<RectI> &p_rects
) {
  int loc_bottom = bottom_boundary;
  if (!stripe.gridded_rows_.empty()) {
    loc_bottom = std::min(loc_bottom, stripe.gridded_rows_[0].LLY());
  }
  int loc_top = top_boundary;
  if (!stripe.gridded_rows_.empty()) {
    loc_top = std::max(loc_top, stripe.gridded_rows_.back().URY());
  }

  std::vector<int> pn_edge_list;
  if (stripe.is_bottom_up_) {
    pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
    pn_edge_list.push_back(loc_bottom);
  } else {
    pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
    pn_edge_list.push_back(loc_top);
  }
  for (auto &cluster: stripe.gridded_rows_) {
    pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
  }
  if (stripe.is_bottom_up_) {
    pn_edge_list.push_back(loc_top);
  } else {
    pn_edge_list.push_back(loc_bottom);
    std::reverse(pn_edge_list.begin(), pn_edge_list.end());
  }

  bool is_p_well_rect;
  if (stripe.gridded_rows_.empty()) {
    is_p_well_rect = stripe.is_first_row_orient_N_;
  } else {
    is_p_well_rect = stripe.gridded_rows_[0].IsOrientN();
  }
  int lx = stripe.LLX();
  int ux = stripe.URX();
  int ly;
  int uy;
  int rect_count = (int) pn_edge_list.size() - 1;
  for (int i = 0; i < rect_count; ++i) {
    ly = pn_edge_list[i];
    uy = pn_edge_list[i + 1];
    if (is_p_well_rect) {
      p_rects.emplace_back(lx, ly, ux, uy);
    } else {
      n_rects.emplace_back(lx, ly, ux, uy);
    }
    is_p_well_rect = !is_p_well_rect;
  }
}

void GenMATLABWellFillingTable(
    std::string const &base_file_name,
    std::vector<ClusterStripe> &col_list,
    int bottom_boundary,
    int top_boundary,
    int well_emit_mode
) {
  std::string p_file = base_file_name + "_pwell.txt";
  std::ofstream ostp(p_file.c_str());
  DaliExpects(ostp.is_open(), "Cannot open output file: " + p_file);

  std::string n_file = base_file_name + "_nwell.txt";
  std::ofstream ostn(n_file.c_str());
  DaliExpects(ostn.is_open(), "Cannot open output file: " + n_file);

  for (auto &col: col_list) {
    for (auto &stripe: col.stripe_list_) {
      std::vector<RectI> n_rects;
      std::vector<RectI> p_rects;
      CollectWellFillingRects(
          stripe,
          bottom_boundary, top_boundary,
          n_rects, p_rects
      );
      if (well_emit_mode != 1) {
        for (auto &rect: p_rects) {
          SaveMatlabPatchRect(
              ostp,
              rect.LLX(), rect.LLY(), rect.URX(), rect.URY()
          );
        }
      }
      if (well_emit_mode != 2) {
        for (auto &rect: n_rects) {
          SaveMatlabPatchRect(
              ostn,
              rect.LLX(), rect.LLY(), rect.URX(), rect.URY()
          );
        }
      }
    }
  }
  ostp.close();
  ostn.close();
}

/****
 * @brief return the weight for optimal anchor
 *
 * Important: must return 0 when i is 0
 *
 * @param i
 * @return
 */
double GetOptimalAnchorWeight(int i) {
  return i * 0.1;
  //return i * i * 0.01;
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
  void UpdateBlockLocation();
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

void BlkDispVarSegment::UpdateBlockLocation() {
  int cur_loc = lx_;
  for (auto &var: vars_) {
    var->SetSolution(cur_loc);
    var->UpdateBlkLocation();
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
    seg.UpdateBlockLocation();
  }
}

}
