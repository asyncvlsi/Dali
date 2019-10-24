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
  std::vector<Block> &block_list = *BlockList();
  auto blk_aux = (MDBlkAux *)blk.Aux();
  int blk_num = blk.Num();
  BinIndex ll = blk_aux->LLIndex();
  BinIndex ur = blk_aux->URIndex();

  std::unordered_set<int> near_blk_set;
  for (int i=ll.x; i<=ur.x; ++i) {
    for (int j=ll.y; j<=ur.y; ++j) {
      for (auto &&num: bin_matrix[i][j].block_set) {
        if (num == blk_num) continue;
        near_blk_set.insert(num);
      }
    }
  }

  double total_overlap = 0;
  for (auto &&num: near_blk_set) {
    total_overlap += blk.OverlapArea(block_list[num]);
  }

  return total_overlap;
}

double DPSimAnneal::SwapCost(Block &blk1, Block &blk2, double hpwl_old, double everlap_old) {

}

void DPSimAnneal::SwapLoc(Block &blk1, Block &blk2) {

}

void DPSimAnneal::GlobalSwap() {
  std::vector<Block> &block_list = *BlockList();
  double cost, best_num, best_cost;
  double hpwl_before_swap, overlap_before_swap;
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
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start DPSimAnneal\n";
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