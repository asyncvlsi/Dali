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
 *  1. the starting point of available space
 *  2. the distant the the previous plug
 *  3. the type of well exposed at the starting point
 * ****/
struct Row {
  int start;
  int dist;
  bool is_n;
  explicit Row(int start_init = 0, int dist_init = INT_MAX, bool is_n_init = false)
      : start(start_init), dist(dist_init), is_n(is_n_init) {}
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

  std::vector<Row> all_rows_;
  std::set<int> p_n_boundary;

 public:
  WellLegalizer();

  void InitWellLegalizer();

  static void SwitchToPlugType(Block &block);
  bool IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row) override;
  void UseSpaceLeft(Block const &block) override;
  void UpdatePNBoundary(Block const &block);
  bool FindLocation(Block &block, int2d &res);
  void WellPlace(Block &block);

  bool IsCurrentLocWellRuleClean(int p_row, Value2D<int> &loc, int lo_row, int hi_row);
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height) override ;
  int WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row) override ;
  bool FindLocLeft(Value2D<int> &loc, int width, int height) override ;
  bool WellLegalizationLeft();

  bool WellLegalizationRight();

  void StartPlacement() override;
};


#endif //DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
