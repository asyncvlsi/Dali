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
#ifndef DALI_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
#define DALI_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_

#include <set>
#include <vector>

#include "dali/common/misc.h"
#include "dali/placer/legalizer/LGTetrisEx.h"

namespace dali {

/****
 * the structure row contains the following information:
 *  1. the distant the the previous plug
 *  2. the type of well exposed at the starting point
 * ****/
struct RowWellStatus {
  int dist;
  int cluster_len;
  bool is_n;
  explicit RowWellStatus(int dist_init = INT_MAX, int cluster_len_init = 0,
                         bool is_n_init = false)
      : dist(dist_init), cluster_len(cluster_len_init), is_n(is_n_init) {}
  bool IsNWell() const { return is_n; }
  bool IsPWell() const { return !is_n; }
  int ClusterLength() const { return cluster_len; }
};

class WellLegalizer : public LGTetrisEx {
 private:
  int n_max_plug_dist_;
  int p_max_plug_dist_;

  int nn_spacing_;
  int pp_spacing_;
  int np_spacing_;

  int n_min_width_;
  int p_min_width_;

  int abutment_benefit_ = 0;

  std::vector<RowWellStatus> row_well_status_;
  std::vector<Value2D<int>> init_loc_;
  std::set<int> p_n_boundary_;

  double k_distance_ = 1;
  double k_min_width_ = 1;
  double well_mis_align_cost_factor_;

 public:
  WellLegalizer();

  void InitWellLegalizer();

  void UpdatePNBoundary(Block const &block);
  bool FindLocation(Block &block, int2d &res);
  void WellPlace(Block &block);

  void MarkSpaceWellLeft(Block const &block, int p_row);
  bool IsCurLocWellDistanceLeft(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurLocWellMinWidthLeft(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height,
                             int p_row);
  bool IsCurrentLocLegalLeft(int loc_x, int width, int lo_row, int hi_row,
                             int p_row);
  bool FindLocLeft(Value2D<int> &loc, int num, int width, int height,
                   int p_row);
  bool WellLegalizationLeft();

  void MarkSpaceWellRight(Block const &block, int p_row);
  bool IsCurLocWellDistanceRight(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurLocWellMinWidthRight(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height,
                              int p_row);
  bool IsCurrentLocLegalRight(int loc_x, int width, int lo_row, int hi_row,
                              int p_row);
  bool FindLocRight(Value2D<int> &loc, int num, int width, int height,
                    int p_row);
  bool WellLegalizationRight();

  bool StartPlacement() override;
};

}  // namespace dali

#endif  // DALI_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
