//
// Created by Yihang on 10/21/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_DPSWAP_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_DPSWAP_H_

#include "MDPlacer.h"
#include <unordered_set>
#include "MDPlacer/bin.h"

class DPSwap: public MDPlacer {
 private:
  double hpwl_converge_criterion_ = 0.01;
  double global_swap_threshold;
 public:
  DPSwap();
  void SingleSegmentCluster();
  void FindOptimalRegion(Block &blk, std::unordered_set<int> &optimal_region);
  void UpdateSwapThreshold(double hpwl);
  double GetOverlap(Block &blk);
  double SwapCostChange(Block &blk1, Block &blk2, double cell_cost);
  void SwapLoc(Block &blk1, Block &blk2);
  void GlobalSwap();
  void VerticalSwap();
  void LocalReOrder();
  void StartPlacement() override;
};

inline void DPSwap::UpdateSwapThreshold(double hpwl) {
  global_swap_threshold = hpwl/100;
}

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_DPSWAP_H_
