//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_

#include <set>
#include <vector>

#include "common/misc.h"
#include "placer/legalizer/LGTetrisEx.h"

/****
 * the structure row contains the following information:
 *  1. the distant the the previous plug
 *  2. the type of well exposed at the starting point
 * ****/
struct RowWellStatus {
  int dist;
  bool is_n;
  explicit RowWellStatus(int dist_init = INT_MAX, bool is_n_init = false)
      : dist(dist_init), is_n(is_n_init) {}
  bool IsNWell() const { return is_n; }
  bool IsPWell() const { return !is_n; }
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

  static void SwitchToPlugType(Block &block);
  void UpdatePNBoundary(Block const &block);
  bool FindLocation(Block &block, int2d &res);
  void WellPlace(Block &block);

  void UseSpaceLeft(Block const &block) override;
  bool IsCurLocWellDistanceLeft(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurLocWellMinWidthLeft(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height, int p_row);
  bool IsCurrentLocLegalLeft(int loc_x, int width, int lo_row, int hi_row, int p_row);
  bool FindLocLeft(Value2D<int> &loc, int num, int width, int height, int p_row);
  bool WellLegalizationLeft();

  void UseSpaceRight(Block const &block) override;
  bool IsCurLocWellDistanceRight(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurLocWellMinWidthRight(int loc_x, int lo_row, int hi_row, int p_row);
  bool IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height, int p_row);
  bool IsCurrentLocLegalRight(int loc_x, int width, int lo_row, int hi_row, int p_row);
  bool FindLocRight(Value2D<int> &loc, int num, int width, int height, int p_row);
  bool WellLegalizationRight();

  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
