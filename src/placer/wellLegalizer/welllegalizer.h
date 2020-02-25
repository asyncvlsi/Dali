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
};

class WellLegalizer : public LGTetrisEx {
 private:
  int n_max_plug_dist_;
  int p_max_plug_dist_;

  int nn_spacing;
  int pp_spacing;
  int np_spacing;

  int n_min_width;
  int p_min_width;

  int abutment_benefit = 0;

  std::vector<RowWellStatus> row_well_status_;
  std::set<int> p_n_boundary;

  double well_mis_align_cost_factor;

 public:
  WellLegalizer();

  void InitWellLegalizer();

  static void SwitchToPlugType(Block &block);
  void UpdatePNBoundary(Block const &block);
  bool FindLocation(Block &block, int2d &res);
  void WellPlace(Block &block);

  void UseSpaceLeft(Block const &block) override;
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height, int p_row);
  bool IsCurrentLocLegalLeft(int loc_x, int width, int lo_row, int hi_row, int p_row);
  bool FindLocLeft(Value2D<int> &loc, int width, int height, int p_row);
  bool WellLegalizationLeft();

  void UseSpaceRight(Block const &block) override;
  bool IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height, int p_row);
  bool IsCurrentLocLegalRight(int loc_x, int width, int lo_row, int hi_row, int p_row);
  bool FindLocRight(Value2D<int> &loc, int width, int height, int p_row);
  bool WellLegalizationRight();

  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
