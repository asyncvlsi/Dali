//
// Created by Yihang Yang on 2/28/20.
//

#include "clusterwelllegalizer.h"

#include <algorithm>

BlockCluster::BlockCluster(int well_extension_init, int plug_width_init) :
    well_extension(well_extension_init),
    plug_width(plug_width_init) {}

void BlockCluster::AppendBlock(Block &block) {
  if (blk_ptr_list.empty()) {
    lx = int(block.LLX());
    ly = int(block.LLY());
    width = block.Width() + well_extension * 2 + plug_width;
    height = block.Height() + well_extension * 2;
    blk_ptr_list.push_back(&block);
  }
  width += block.Width();
  if (block.Height() > height) {
    height = block.Height() + well_extension * 2;
  }

}

void BlockCluster::OptimizeHeight() {
  /****
   * This function aligns all N/P well boundaries of cells inside a cluster
   * ****/

}

void BlockCluster::UpdateBlockLocationX() {
  int current_loc = well_extension;
  for (auto &blk_ptr: blk_ptr_list) {
    blk_ptr->SetLLX(current_loc);
    current_loc += blk_ptr->Width();
  }
}

ClusterWellLegalizer::ClusterWellLegalizer() : LGTetrisEx() {}

void ClusterWellLegalizer::InitializeClusterLegalizer() {
  InitLegalizer();
  row_cluster_status_.resize(tot_num_rows_, nullptr);
}

BlockCluster *ClusterWellLegalizer::FindClusterForBlock(Block &block) {
  /****
   * Returns the pointer to the cluster which gives the minimum cost.
   * Cost function is the displacement.
   * If there is no cluster nearby, create a new cluster, and return the new cluster.
   * If there are clusters nearby, but the displacement is too large, create a new cluster, and return the new cluster.
   * If there are clusters nearby, and one of them gives the smallest cost, returns that cluster.
   * ****/

  int lo_row = StartRow((int) block.LLY());
  int hi_row = EndRow((int) block.URY());

  BlockCluster *res_cluster = nullptr;
  for (int i = lo_row; i <= hi_row; ++i) {
    
  }

}

void ClusterWellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Well Legalization complete!\n"
              << "\033[0m";
  }

  /****---->****/
  InitializeClusterLegalizer();

  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  for (int i = 0; i < sz; ++i) {
    auto &block = block_list[index_loc_list_[i].num];

    if (block.IsFixed()) continue;

    BlockCluster *cluster = FindClusterForBlock(block);
  }


  /****<----****/
  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "(wall time: "
              << wall_time << "s, cpu time: "
              << cpu_time << "s)\n";
  }

  ReportMemory(LOG_CRITICAL);
}