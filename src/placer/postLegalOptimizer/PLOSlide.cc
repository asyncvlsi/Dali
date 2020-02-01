//
// Created by Yihang Yang on 1/29/20.
//

#include "PLOSlide.h"

#include <algorithm>

PLOSlide::PLOSlide() {}

void PLOSlide::InitPostLegalOptimizer() {
  row_start_.assign(top_ - bottom_ + 1, left_);
  net_aux_list_.reserve(circuit_->net_list.size());

  for (auto &&net: circuit_->net_list) {
    net_aux_list_.emplace_back(&net);
  }
}

void PLOSlide::FindOptimalRegionX(Block &block, double &start, double &end) {
  Pin *pin;
  double pin_offset_x;
  std::vector<double> net_bound_x;

  double lo, hi;
  for (auto &net_num: block.net_list) {
    circuit_->net_list[net_num].XBoundExclude(block, lo, hi);
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
    end = net_bound_x[mid-1];
  }
}

void PLOSlide::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start Post-Legalization Optimization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitPostLegalOptimizer();

  row_start_.assign(row_start_.size(), right_);
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  double opt_region_s;
  double opt_region_e;
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    FindOptimalRegionX(block, opt_region_s, opt_region_e);
  }

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
}