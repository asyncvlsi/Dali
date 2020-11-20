//
// Created by Yihang Yang on 10/21/2019.
//

#include "DPSwap.h"

DPSwap::DPSwap() {
  global_swap_threshold = 0;
}

void DPSwap::SingleSegmentCluster() {

}

void DPSwap::FindOptimalRegion(Block &blk, std::unordered_set<int> &optimal_region) {

}

double DPSwap::GetOverlap(Block &blk) {
  std::vector<Block> &block_list = *BlockList();
  auto blk_aux = (MDBlkAux *) blk.AuxPtr();
  int blk_num = blk.Num();
  BinIndex ll = blk_aux->LLIndex();
  BinIndex ur = blk_aux->URIndex();

  std::unordered_set<int> near_blk_set;
  for (int i=ll.x; i<=ur.x; ++i) {
    for (int j=ll.y; j<=ur.y; ++j) {
      for (auto &num: bin_matrix[i][j].block_set) {
        if (num == blk_num) continue;
        near_blk_set.insert(num);
      }
    }
  }

  double total_overlap = 0;
  for (auto &num: near_blk_set) {
    total_overlap += blk.OverlapArea(block_list[num]);
  }

  return total_overlap;
}

double DPSwap::SwapCostChange(Block &blk1, Block &blk2, double cell_cost) {
  double cost_old = GetOverlap(blk2) + GetBlkHPWL(blk2) + cell_cost;

  SwapLoc(blk1, blk2);
  double cost_new = GetOverlap(blk1) + GetOverlap(blk2) + GetBlkHPWL(blk1) + GetBlkHPWL(blk2);
  SwapLoc(blk1, blk2);
  return cost_old - cost_new;
}

void DPSwap::SwapLoc(Block &blk1, Block &blk2) {
  blk1.SwapLoc(blk2);
  UpdateBin(blk1);
  UpdateBin(blk2);
}

void DPSwap::GlobalSwap() {
  std::vector<Block> &block_list = *BlockList();
  double benefit, best_num, best_benefit;
  double cell_cost;
  bool found_good_enough_swap;
  for (auto &blk: block_list) {
    std::unordered_set<int> optimal_region;
    FindOptimalRegion(blk, optimal_region);
    best_benefit = 0;
    cell_cost = GetBlkHPWL(blk) + GetOverlap(blk);
    found_good_enough_swap = false;
    best_num = -1;
    for (auto &num: optimal_region) {
      benefit = SwapCostChange(blk, block_list[num], cell_cost);
      if (benefit > best_benefit) {
        best_num = num;
        best_benefit = benefit;
      }
      if (benefit < global_swap_threshold) {
        SwapLoc(blk, block_list[num]);
        found_good_enough_swap = true;
        break;
      }
    }
    if (!found_good_enough_swap && best_benefit < 0 && best_num > 0 && best_num != blk.Num()) {
      SwapLoc(blk, block_list[best_num]);
    }
  }
}

void DPSwap::VerticalSwap() {


}

void DPSwap::LocalReOrder() {

}

bool DPSwap::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start DPSwap\n";
  }
  CreateBlkAuxList();
  InitGridBin();
  UpdateBinMatrix();
  double hpwl = 1e30;
  double hpwl_new;
  bool hpwl_converge = false;
  SingleSegmentCluster();
  for (int i=0; ; ++i) {
    GlobalSwap();
    VerticalSwap();
    LocalReOrder();
    hpwl_new = WeightedHPWL();
    hpwl_converge = (std::fabs(1 - hpwl_new/hpwl) < hpwl_converge_criterion_);
    UpdateSwapThreshold(hpwl_new);
    if (i>5 && hpwl_converge) break;
  }
  for (int i=0; ; ++i) {
    SingleSegmentCluster();
    hpwl_new = WeightedHPWL();
    hpwl_converge = (std::fabs(1 - hpwl_new/hpwl) < hpwl_converge_criterion_);
    if (i>5 && hpwl_converge) break;
  }

  return true;
}