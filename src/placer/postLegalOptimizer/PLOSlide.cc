//
// Created by Yihang Yang on 1/29/20.
//

#include "PLOSlide.h"

#include <algorithm>

PLOSlide::PLOSlide() {}

void PLOSlide::InitPostLegalOptimizer() {
  row_start_.assign(top_ - bottom_ + 1, left_);
  net_aux_list_.reserve(NetList()->size());

  for (auto &net: *NetList()) {
    net_aux_list_.emplace_back(&net);
  }
}

void PLOSlide::FindOptimalRegionX(Block &block, int &start, int &end) {
  Pin *pin;
  double pin_offset_x;
  std::vector<double> net_bound_x;

  double lo, hi;
  auto &net_list = *(NetList());
  for (auto &net_num: block.net_list) {
    net_list[net_num].XBoundExclude(block, lo, hi);
    pin = net_aux_list_[net_num].GetPin(block);
    pin_offset_x = pin->OffsetX(block.Orient());
    net_bound_x.push_back(lo - pin_offset_x);
    net_bound_x.push_back(hi - pin_offset_x);
  }

  std::sort(net_bound_x.begin(), net_bound_x.end()); // sort
  auto p_x = std::unique(net_bound_x.begin(), net_bound_x.end()); // compact
  net_bound_x.erase(p_x, net_bound_x.end()); // shrink

  int sz = net_bound_x.size();
  int mid = sz / 2;
  if (sz % 2 == 1) {
    start = net_bound_x[mid];
    end = net_bound_x[mid];
  } else {
    start = net_bound_x[mid];
    end = net_bound_x[mid - 1];
  }
}

void PLOSlide::MoveBlkTowardOptimalRegion(Block &block, int start, int end) {
  /****
   * 1. In the x direction, the optimal region is [start, end]
   * 2. If start >= current.x, do not move this block, because this member function only moves blocks leftwards
   * 3. If current.x in [start, end], move this block leftwards as much as possible, but larger than start
   * 4. If current.x is larger than end, move this block leftwards as much as possible, but larger than start
   * 5. 3&4 means => if (current.x > start), move this block leftward as much as possible, but larger than start
   * ****/
  int start_row = int(block.LLY() - RegionBottom());
  int end_row = int(start_row + block.Height() - 1);

  assert(end_row >= int(row_start_.size()));

  if (block.LLX() > start) {
    int final_loc = start;
    for (int i = start_row; i <= end_row; ++i) {
      final_loc = std::max(final_loc, row_start_[i]);
    }
    block.SetLLX(final_loc);
  }

  int end_x = int(block.URX());
  for (int i = start_row; i <= end_row; ++i) {
    row_start_[i] = end_x;
  }
}

void PLOSlide::OptimizationFromLeft() {

  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  row_start_.assign(row_start_.size(), RegionLeft());

  int opt_region_start;
  int opt_region_end;
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    FindOptimalRegionX(block, opt_region_start, opt_region_end);

    MoveBlkTowardOptimalRegion(block, opt_region_start, opt_region_end);
  }

}

void PLOSlide::MoveBlkTowardOptimalRegionRight(Block &block, int start, int end) {
  /****
   * 1. In the x direction, the optimal region is [start, end]
   * 2. If current.x > end, do not move this block, because this member function only moves blocks rightwards
   * 3. If current.x in [start, end], move this block rightwards as much as possible, but smaller than end
   * 4. If current.x is smaller than start, move this block rightwards as much as possible, but smaller than start
   * 5. 3&4 means => if (current.x < end), move this block rightward as much as possible, but smaller than start
   * ****/
  int start_row = int(block.LLY() - RegionBottom());
  int end_row = int(start_row + block.Height() - 1);

  assert(end_row >= int(row_start_.size()));

  if (block.URX() < end) {
    int final_loc = end;
    for (int i = start_row; i <= end_row; ++i) {
      final_loc = std::min(final_loc, row_start_[i]);
    }
    block.SetURX(final_loc);
  }

  int end_x = int(block.LLX());
  for (int i = start_row; i <= end_row; ++i) {
    row_start_[i] = end_x;
  }
}

void PLOSlide::OptimizationFromRight() {
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].URX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(),
            index_loc_list_.end(),
            [](const IndexLocPair<int> &lhs, const IndexLocPair<int> &rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });

  row_start_.assign(row_start_.size(), RegionRight());

  int opt_region_start;
  int opt_region_end;
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    FindOptimalRegionX(block, opt_region_start, opt_region_end);

    MoveBlkTowardOptimalRegionRight(block, opt_region_start, opt_region_end);
  }
}

bool PLOSlide::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Post-Legalization Optimization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitPostLegalOptimizer();

  OptimizationFromLeft();
  OptimizationFromRight();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Post-Legalization Optimization complete\n"
              << "\033[0m";
  }

  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "(wall time: "
              << wall_time << "s, cpu time: "
              << cpu_time << "s)\n";
  }

  ReportMemory(LOG_CRITICAL);

  return true;
}
