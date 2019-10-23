//
// Created by Yihang on 10/21/2019.
//

#include "DPSimAnneal.h"

DPSimAnneal::DPSimAnneal() {
  global_swap_threshold = 0;
}

void DPSimAnneal::SingleSegmentCluster() {

}

void DPSimAnneal::FindOptimalRegion(Block &blk, std::unordered_set<int> &optimal_region) {

}

double DPSimAnneal::GetOverlap(Block &blk) {

}

double DPSimAnneal::SwapCost(Block &blk1, Block &blk2, double hpwl_old, double everlap_old) {

}

void DPSimAnneal::SwapLoc(Block &blk1, Block &blk2) {

}

void DPSimAnneal::GlobalSwap() {
  std::vector<Block> &block_list = *BlockList();
  double cost, best_num, best_cost;
  int hpwl_before_swap, overlap_before_swap;
  bool found_good_enough_swap;
  for (auto &&blk: block_list) {
    std::unordered_set<int> optimal_region;
    FindOptimalRegion(blk, optimal_region);
    best_cost = 1e30;
    hpwl_before_swap = GetBlkHPWL(blk);
    overlap_before_swap = GetOverlap(blk);
    found_good_enough_swap = false;
    best_num = -1;
    for (auto &&num: optimal_region) {
      cost = SwapCost(blk, block_list[num], hpwl_before_swap, overlap_before_swap);
      if (cost < best_cost) {
        best_num = num;
        best_cost = cost;
      }
      if (cost < global_swap_threshold) {
        SwapLoc(blk, block_list[num]);
        found_good_enough_swap = true;
        break;
      }
    }
    if (!found_good_enough_swap && best_cost < 0 && best_num > 0 && best_num != blk.Num()) {
      SwapLoc(blk, block_list[best_num]);
    }
  }
}

void DPSimAnneal::VerticalSwap() {


}

void DPSimAnneal::LocalReOrder() {

}

void DPSimAnneal::StartPlacement() {
  double hpwl = 1e30;
  double hpwl_new;
  bool hpwl_converge = false;
  SingleSegmentCluster();
  for (int i=0; ; ++i) {
    GlobalSwap();
    VerticalSwap();
    LocalReOrder();
    hpwl_new = HPWL();
    hpwl_converge = (std::fabs(1 - hpwl_new/hpwl) < hpwl_converge_criterion_);
    UpdateSwapThreshold(hpwl_new);
    if (i>5 && hpwl_converge) break;
  }
  for (int i=0; ; ++i) {
    SingleSegmentCluster();
    hpwl_new = HPWL();
    hpwl_converge = (std::fabs(1 - hpwl_new/hpwl) < hpwl_converge_criterion_);
    if (i>5 && hpwl_converge) break;
  }
}