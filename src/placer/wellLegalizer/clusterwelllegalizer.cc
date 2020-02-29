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
    lx = int(block.LLX()) - well_extension;
    modified_lx = lx - well_extension - plug_width;
    ly = int(block.LLY()) - well_extension;
    width = block.Width() + well_extension * 2 + plug_width;
    height = block.Height() + well_extension * 2;
  } else {
    width += block.Width();
    if (block.Height() > height) {
      ly -= (block.Height() - height + 1) / 2;
      height = block.Height() + well_extension * 2;
    }
  }
  blk_ptr_list.push_back(&block);
}

void BlockCluster::OptimizeHeight() {
  /****
   * This function aligns all N/P well boundaries of cells inside a cluster
   * ****/

}

void BlockCluster::UpdateBlockLocation() {
  int current_loc = lx;
  for (auto &blk_ptr: blk_ptr_list) {
    blk_ptr->SetLLX(current_loc);
    blk_ptr->SetCenterY(this->CenterY());
    current_loc += blk_ptr->Width();
  }
}

ClusterWellLegalizer::ClusterWellLegalizer() : LGTetrisEx() {}

void ClusterWellLegalizer::InitializeClusterLegalizer() {
  InitLegalizer();
  row_to_cluster_.resize(tot_num_rows_, nullptr);
}

BlockCluster *ClusterWellLegalizer::CreateNewCluster() {
  auto *new_cluster = new BlockCluster(well_extension, plug_width);
  cluster_set.insert(new_cluster);
  return new_cluster;
}

void ClusterWellLegalizer::AddBlockToCluster(Block &block, BlockCluster *cluster) {
  /****
   * Append the @param block to the @param cluster
   * Update the row_to_cluster_
   * ****/
  cluster->AppendBlock(block);
  int lo_row = StartRow((int) block.LLY());
  int hi_row = EndRow((int) block.URY());

  for (int i = lo_row; i <= hi_row; ++i) {
    row_to_cluster_[i] = cluster;
  }
}

BlockCluster *ClusterWellLegalizer::FindClusterForBlock(Block &block) {
  /****
   * Returns the pointer to the cluster which gives the minimum cost.
   * Cost function is the displacement.
   * If there is no cluster nearby, create a new cluster, and return the new cluster.
   * If there are clusters nearby, but the displacement is too large, create a new cluster, and return the new cluster.
   * If there are clusters nearby, and one of them gives the smallest cost but the length of cluster is not too long, returns that cluster.
   * ****/
  int init_x = (int) block.LLX();
  int init_y = (int) block.LLY();
  int height = block.Height();

  int max_search_row = MaxRow(height);

  int search_start_row = std::max(0, LocToRow(init_y - 2 * height));
  int search_end_row = std::min(max_search_row, LocToRow(init_y + 3 * height));

  BlockCluster *res_cluster = nullptr;
  BlockCluster *pre_cluster = nullptr;
  BlockCluster *cur_cluster = nullptr;

  double min_cost = DBL_MAX;

  for (int i = search_start_row; i <= search_end_row; ++i) {
    cur_cluster = row_to_cluster_[i];
    if (cur_cluster == pre_cluster) {
      pre_cluster = cur_cluster;
      continue;
    }

    double tmp_cost = std::fabs(cur_cluster->InnerUX() - init_x) + std::fabs(cur_cluster->CenterY() - block.Y());
    if (cur_cluster->Width() + block.Width() > max_well_length) {
      tmp_cost = DBL_MAX;
    }

    if (tmp_cost < min_cost) {
      min_cost = tmp_cost;
      res_cluster = cur_cluster;
    }

  }

  if (min_cost > new_cluster_cost_threshold) {
    res_cluster = CreateNewCluster();
  }

  return res_cluster;
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
    AddBlockToCluster(block, cluster);
  }

  for (auto &cluster_ptr : cluster_set) {
    cluster_ptr->UpdateBlockLocation();
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