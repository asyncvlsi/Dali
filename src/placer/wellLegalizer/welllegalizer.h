//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_

#include "placer/placer.h"
#include "common/misc.h"
#include <vector>
#include <set>

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
  explicit Row(int start_init = 0, int dist_init = INT_MAX, bool is_n_init = false): start(start_init), dist(dist_init), is_n(is_n_init) {}
};

class WellLegalizer: public Placer {
 private:
  int n_max_plug_dist_;
  int p_max_plug_dist_;
  std::vector<Row> all_rows_;
  std::set<int> p_n_boundary;
  std::vector<IndexLocPair<int>> index_loc_list_;
 public:
  void InitWellLegalizer();
  static void SwitchToPlugType(Block &block);
  bool IsSpaceAvail(int lx, int ly, int width, int height);
  void UseSpace(int lx, int ly, int width, int height);
  void FindLocation(Block &block, int &lx, int &ly);
  void WellPlace(Block &block);
  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
