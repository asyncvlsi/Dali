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
  int32_t dist;
  int32_t cluster_len;
  bool is_n;
  explicit RowWellStatus(
      int32_t dist_init = INT_MAX,
      int32_t cluster_len_init = 0,
      bool is_n_init = false
  )
      : dist(dist_init), cluster_len(cluster_len_init), is_n(is_n_init) {}
  bool IsNWell() const { return is_n; }
  bool IsPWell() const { return !is_n; }
  int32_t ClusterLength() const { return cluster_len; }
};

class WellLegalizer : public LGTetrisEx {
 private:
  int32_t n_max_plug_dist_;
  int32_t p_max_plug_dist_;

  int32_t nn_spacing_;
  int32_t pp_spacing_;
  int32_t np_spacing_;

  int32_t n_min_width_;
  int32_t p_min_width_;

  int32_t abutment_benefit_ = 0;

  std::vector<RowWellStatus> row_well_status_;
  std::vector<Value2D < int32_t>> init_loc_;
  std::set<int32_t> p_n_boundary_;

  double k_distance_ = 1;
  double k_min_width_ = 1;
  double well_mis_align_cost_factor_;

 public:
  WellLegalizer();

  void InitWellLegalizer();

  void UpdatePNBoundary(Block const &block);
  bool FindLocation(Block &block, int2d &res);
  void WellPlace(Block &block);

  void MarkSpaceWellLeft(Block const &block, int32_t p_row);
  bool IsCurLocWellDistanceLeft(int32_t loc_x, int32_t lo_row, int32_t hi_row, int32_t p_row);
  bool IsCurLocWellMinWidthLeft(int32_t loc_x, int32_t lo_row, int32_t hi_row, int32_t p_row);
  bool IsCurrentLocLegalLeft(
      Value2D<int32_t> &loc,
      int32_t width,
      int32_t height,
      int32_t p_row
  );
  bool IsCurrentLocLegalLeft(
      int32_t loc_x,
      int32_t width,
      int32_t lo_row,
      int32_t hi_row,
      int32_t p_row
  );
  bool FindLocLeft(
      Value2D<int32_t> &loc,
      int32_t num,
      int32_t width,
      int32_t height,
      int32_t p_row
  );
  bool WellLegalizationLeft();

  void MarkSpaceWellRight(Block const &block, int32_t p_row);
  bool IsCurLocWellDistanceRight(
      int32_t loc_x,
      int32_t lo_row,
      int32_t hi_row,
      int32_t p_row
  );
  bool IsCurLocWellMinWidthRight(
      int32_t loc_x,
      int32_t lo_row,
      int32_t hi_row,
      int32_t p_row
  );
  bool IsCurrentLocLegalRight(
      Value2D<int32_t> &loc,
      int32_t width,
      int32_t height,
      int32_t p_row
  );
  bool IsCurrentLocLegalRight(
      int32_t loc_x,
      int32_t width,
      int32_t lo_row,
      int32_t hi_row,
      int32_t p_row
  );
  bool FindLocRight(
      Value2D<int32_t> &loc,
      int32_t num,
      int32_t width,
      int32_t height,
      int32_t p_row
  );
  bool WellLegalizationRight();

  bool StartPlacement() override;
};

}

#endif //DALI_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
