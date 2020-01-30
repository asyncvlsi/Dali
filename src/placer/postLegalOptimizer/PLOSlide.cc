//
// Created by Yihang Yang on 1/29/20.
//

#include "PLOSlide.h"

#include <algorithm>

void PLOSlide::InitPostLegalOptimizer() {

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

  std::vector<Net> &net_list = *NetList();
  int net_sz = int(net_list.size());

  for (int i = 0; i < net_sz; ++i) {
    net_aux_list_[i].UpdateMaxMinX();
  }

  int pin_num;
  double pin_offset_x;
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    std::vector<double> net_bound_x;
    for (auto &net_num: block.net_list) {
      pin_num = net_aux_list_[net_num].GetPinNum(block);
      pin_offset_x = block.Type()->pin_list[pin_num].XOffset(block.Orient());
      net_bound_x.push_back(net_aux_list_[net_num].MinX() - pin_offset_x);
      net_bound_x.push_back(net_aux_list_[net_num].MaxX() - pin_offset_x);
    }

    std::sort(net_bound_x.begin(), net_bound_x.end()); // sort
    auto p_x = std::unique(net_bound_x.begin(), net_bound_x.end()); // compact
    net_bound_x.erase(p_x, net_bound_x.end()); // shrink
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