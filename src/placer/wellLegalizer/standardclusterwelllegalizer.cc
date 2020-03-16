//
// Created by Yihang Yang on 3/14/20.
//

#include "standardclusterwelllegalizer.h"

#include <algorithm>

void Cluster::UpdateLocY() {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->SetCenterY(CenterY());
  }
}

void Cluster::LegalizeX(int left) {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = left;
  for (auto &blk: blk_list_) {
    blk->SetLLX(current_x);
    current_x += blk->Width();
  }
}

void ClusterColumn::AppendBlock(Block &blk) {
  bool is_new_cluster_needed = clusters_.empty();
  if (!is_new_cluster_needed) {
    bool is_not_in_top_cluster = clusters_.back().URY() <= blk.LLY();
    bool is_top_cluster_full = clusters_.back().UsedSize() + blk.Width() > width_;
    is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
  }

  int width = blk.Width();
  int height = blk.Height();
  int init_y = (int) std::round(blk.LLY());

  if (is_new_cluster_needed) {
    clusters_.emplace_back();
    auto &top_cluster = clusters_.back();
    top_cluster.blk_list_.reserve(clus_blk_cap_);
    top_cluster.blk_list_.push_back(&blk);
    top_cluster.SetUsedSize(width);
    top_cluster.SetLLY(init_y);
    top_cluster.SetHeight(height);
  } else {
    auto &top_cluster = clusters_.back();
    top_cluster.blk_list_.push_back(&blk);
    top_cluster.UseSpace(width);
    if (height > top_cluster.Height()) {
      top_cluster.SetHeight(height);
    }
  }
}

void ClusterColumn::LegalizeCluster() {
  for (auto &cluster: clusters_) {
    cluster.LegalizeX(lx_);
  }
}

StandardClusterWellLegalizer::StandardClusterWellLegalizer() {
  max_unplug_length_ = 0;
  plug_cell_width_ = 0;
  max_cluster_width_ = 0;
  col_width_ = 0;
  tot_col_num_ = 0;
}

void StandardClusterWellLegalizer::Init() {
  // parameters fetching
  auto tech_params = circuit_->GetTech();
  Assert(tech_params != nullptr, "No tech info found, well legalization cannot proceed!\n");
  auto n_well_layer = tech_params->GetNLayer();
  printf("Well max plug distance: %2.2e um \n", n_well_layer->MaxPlugDist());
  printf("GridValueX: %2.2e um\n", circuit_->GetGridValueX());
  max_unplug_length_ = std::floor(n_well_layer->MaxPlugDist() / circuit_->GetGridValueX());
  plug_cell_width_ = 4;

  max_cluster_width_ = max_unplug_length_ + plug_cell_width_;
  col_width_ = max_unplug_length_;
  if (col_width_ > RegionWidth()) {
    col_width_ = RegionWidth();
  }
  tot_col_num_ = std::ceil(RegionWidth() / (double) col_width_);
  printf("Total number of cluster columns: %d\n", tot_col_num_);

  int max_clusters_per_col = RegionHeight() / circuit_->MinHeight();
  clus_cols_.resize(tot_col_num_);
  for (int i = 0; i < tot_col_num_; ++i) {
    clus_cols_[i].clusters_.reserve(max_clusters_per_col);
    clus_cols_[i].lx_ = RegionLeft() + i * col_width_;
    clus_cols_[i].ux_ = std::min(clus_cols_[i].lx_ + col_width_, RegionRight());
    clus_cols_[i].width_ = clus_cols_[i].ux_ - clus_cols_[i].lx_;
    clus_cols_[i].clus_blk_cap_ = clus_cols_[i].Width() / circuit_->MinWidth();
  }
  printf("Maximum possible number of clusters in a column: %d\n", max_clusters_per_col);

  index_loc_list_.resize(circuit_->block_list.size());
}

void StandardClusterWellLegalizer::ClusterBlocks() {
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(),
            index_loc_list_.end(),
            [](const IndexLocPair<int> &lhs, const IndexLocPair<int> &rhs) {
              return (lhs.y < rhs.y) || (lhs.y == rhs.y && lhs.x < rhs.x);
            });

  for (int i = 0; i < sz; ++i) {
    auto &block = block_list[index_loc_list_[i].num];
    if (block.IsFixed()) continue;

    int col_num = LocToCol((int) std::round(block.X()));
    clus_cols_[col_num].AppendBlock(block);
  }

  for (auto &col: clus_cols_) {
    int current_ly = RegionBottom();
    for (auto &cluster: col.clusters_) {
      cluster.SetLLY(current_ly);
      cluster.UpdateLocY();
      current_ly += cluster.Height();
    }
    col.LegalizeCluster();
  }
}

void StandardClusterWellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Standard Cluster Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  /****---->****/
  Init();
  ClusterBlocks();

  /****<----****/

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Standard Cluster Well Legalization complete!\n"
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
  GenMatlabClusterTable("sc_result");
}

void StandardClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);

  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &col: clus_cols_) {
    for (auto &cluster: col.clusters_) {
      ost << col.LLX() << "\t"
          << col.URX() << "\t"
          << col.URX() << "\t"
          << col.LLX() << "\t"
          << cluster.LLY() << "\t"
          << cluster.LLY() << "\t"
          << cluster.URY() << "\t"
          << cluster.URY() << "\n";
    }
  }
  ost.close();
}
