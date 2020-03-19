//
// Created by Yihang Yang on 3/14/20.
//

#include "standardclusterwelllegalizer.h"

#include <algorithm>

void Cluster::ShiftBlockX(int x_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreX(x_disp);
  }
}

void Cluster::ShiftBlockY(int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreY(y_disp);
  }
}

void Cluster::ShiftBlock(int x_disp, int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreX(x_disp);
    blk_ptr->IncreY(y_disp);
  }
}

void Cluster::UpdateLocY() {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->SetCenterY(CenterY());
  }
}

void Cluster::LegalizeCompactX(int left) {
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

void Cluster::LegalizeLooseX(int left, int right) {
  /****
   * Legalize this cluster using the extended Tetris legalization algorithm
   *
   * 1. legalize blocks from left
   * 2. if block contour goes out of the right boundary, legalize blocks from right
   *
   * if the total width of blocks in this cluster is smaller than the width of this cluster,
   * two-rounds legalization is enough to make the final result legal.
   * ****/
  if (blk_list_.empty()) {
    return;
  }
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int block_contour = left;
  int res_x;
  for (auto &blk: blk_list_) {
    res_x = std::max(block_contour, int(blk_list_[0]->LLX()));
    blk->SetLLX(res_x);
    block_contour = int(blk->URX());
  }

  if (block_contour > right) {
    std::sort(blk_list_.begin(),
              blk_list_.end(),
              [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->URX() > blk_ptr1->URX();
              });
    block_contour = right;
    for (auto &blk: blk_list_) {
      res_x = std::min(block_contour, int(blk_list_[0]->URX()));
      blk->SetURX(res_x);
      block_contour = int(blk->LLX());
    }
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
  col_width_ = RegionWidth() / tot_col_num_;
  for (int i = 0; i < tot_col_num_; ++i) {
    clus_cols_[i].lx_ = RegionLeft() + i * col_width_;
    clus_cols_[i].width_ = col_width_;
    clus_cols_[i].contour_ = RegionBottom();
    clus_cols_[i].used_height_ = 0;
    clus_cols_[i].cluster_count_ = 0;
    clus_cols_[i].max_blk_capacity_per_cluster_ = clus_cols_[i].Width() / circuit_->MinWidth();
  }
  clus_cols_[tot_col_num_ - 1].width_ = RegionRight() - clus_cols_[tot_col_num_ - 1].lx_;
  cluster_list_.reserve(tot_col_num_ * max_clusters_per_col);
  printf("Maximum possible number of clusters in a column: %d\n", max_clusters_per_col);

  index_loc_list_.resize(circuit_->block_list.size());
}

void StandardClusterWellLegalizer::AppendBlockToCol(int col_num, Block &blk) {
  auto &col = clus_cols_[col_num];
  bool is_new_cluster_needed = (col.contour_ == RegionBottom());
  if (!is_new_cluster_needed) {
    bool is_not_in_top_cluster = col.contour_ <= blk.LLY();
    bool is_top_cluster_full = col.top_cluster_->UsedSize() + blk.Width() > col.width_;
    is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
  }

  int width = blk.Width();
  int height = blk.Height();
  int init_y = (int) std::round(blk.LLY());
  if (col.contour_ != RegionBottom()) {
    init_y = std::max(init_y, col.contour_);
  }

  Cluster *top_cluster;
  if (is_new_cluster_needed) {
    cluster_list_.emplace_back();
    top_cluster = &(cluster_list_.back());
    top_cluster->blk_list_.reserve(col.max_blk_capacity_per_cluster_);
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->SetUsedSize(width);
    top_cluster->SetLLY(init_y);
    top_cluster->SetHeight(height);
    top_cluster->SetLLX(col.LLX());
    top_cluster->SetWidth(col.Width());

    col.top_cluster_ = top_cluster;
    col.cluster_count_ += 1;
    col.used_height_ += top_cluster->Height();
  } else {
    top_cluster = col.top_cluster_;
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->UseSpace(width);
    if (height > top_cluster->Height()) {
      col.used_height_ += height - top_cluster->Height();
      top_cluster->SetHeight(height);
    }
  }
  col.contour_ = top_cluster->URY();
}

void StandardClusterWellLegalizer::AppendBlockToColClose(int col_num, Block &blk) {
  auto &col = clus_cols_[col_num];
  bool is_new_cluster_needed = (col.contour_ == RegionBottom());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full = col.top_cluster_->UsedSize() + blk.Width() > col.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int height = blk.Height();
  int init_y = (int) std::round(blk.LLY());
  if (col.contour_ != RegionBottom()) {
    init_y = std::max(init_y, col.contour_);
  }

  Cluster *top_cluster;
  if (is_new_cluster_needed) {
    cluster_list_.emplace_back();
    top_cluster = &(cluster_list_.back());
    top_cluster->blk_list_.reserve(col.max_blk_capacity_per_cluster_);
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->SetUsedSize(width);
    top_cluster->SetLLY(init_y);
    top_cluster->SetHeight(height);
    top_cluster->SetLLX(col.LLX());
    top_cluster->SetWidth(col.Width());

    col.top_cluster_ = top_cluster;
    col.cluster_count_ += 1;
    col.used_height_ += top_cluster->Height();
  } else {
    top_cluster = col.top_cluster_;
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->UseSpace(width);
    if (height > top_cluster->Height()) {
      col.used_height_ += height - top_cluster->Height();
      top_cluster->SetHeight(height);
    }
  }
  col.contour_ = top_cluster->URY();
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
    AppendBlockToCol(col_num, block);
  }

  for (auto &col: clus_cols_) {
    col.contour_ = RegionBottom();
  }
  for (auto &cluster: cluster_list_) {
    int col_num = (cluster.LLX() - RegionLeft()) / col_width_;
    auto &col = clus_cols_[col_num];
    col.contour_ += cluster.Height();
    cluster.SetLLY(col.contour_);
    cluster.UpdateLocY();
    cluster.LegalizeCompactX(cluster.LLX());
  }
}

void StandardClusterWellLegalizer::ClusterBlocksLoose() {
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
    AppendBlockToCol(col_num, block);
  }

  for (auto &cluster: cluster_list_) {
    //cluster.SetLLY(current_ly);
    cluster.UpdateLocY();
    cluster.LegalizeLooseX(cluster.LLX(), cluster.URX());
    //current_ly += cluster.Height();
  }
}

void StandardClusterWellLegalizer::ClusterBlocksCompact() {
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
    AppendBlockToColClose(col_num, block);
  }

  for (auto &col: clus_cols_) {
    col.contour_ = RegionBottom();
  }
  for (auto &cluster: cluster_list_) {
    int col_num = (cluster.LLX() - RegionLeft()) / col_width_;
    auto &col = clus_cols_[col_num];
    col.contour_ += cluster.Height();
    cluster.SetLLY(col.contour_);
    cluster.UpdateLocY();
    cluster.LegalizeCompactX(cluster.LLX());
  }
}

void StandardClusterWellLegalizer::TrialClusterLegalization() {
  /****
   * Legalize the location of all clusters using extended Tetris legalization algorithm in columns where usage does not exceed capacity
   * Closely pack the column from bottom to top if its usage exceeds its capacity
   * ****/

  std::vector<std::vector<Cluster *>> clusters_in_column(tot_col_num_);
  for (int i = 0; i < tot_col_num_; ++i) {
    clusters_in_column[i].reserve(clus_cols_[i].cluster_count_);
  }

  for (auto &cluster: cluster_list_) {
    int col_num = LocToCol((int) std::round(cluster.LLX()));
    clusters_in_column[col_num].emplace_back(&cluster);
  }

  // sort clusters in each column based on the lower left corner
  for (int i = 0; i < tot_col_num_; ++i) {
    auto &col = clus_cols_[i];
    auto &cluster_list = clusters_in_column[i];
    if (col.contour_ <= RegionTop()) continue;

    //std::cout << "used height/RegionHeight(): " << col.used_height_ / RegionHeight() << "\n";
    if (col.used_height_ <= RegionHeight()) {
      std::sort(cluster_list.begin(),
                cluster_list.end(),
                [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->LLY() > rhs->LLY());
                });
      int cluster_contour = RegionTop();
      int res_y;
      int init_y;
      for (auto &cluster: cluster_list) {
        init_y = cluster->URY();
        res_y = std::min(cluster_contour, cluster->URY());
        cluster->SetURY(res_y);
        cluster_contour = cluster->LLY();
        cluster->ShiftBlockY(res_y - init_y);
      }
    } else {
      std::sort(clusters_in_column[i].begin(),
                clusters_in_column[i].end(),
                [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->LLY() < rhs->LLY());
                });
      int cluster_contour = RegionBottom();
      int res_y;
      int init_y;
      for (auto &cluster: cluster_list) {
        init_y = cluster->LLY();
        res_y = cluster_contour;
        cluster->SetLLY(res_y);
        cluster_contour += cluster->Height();
        cluster->ShiftBlockY(res_y - init_y);
      }
    }
  }
}

void StandardClusterWellLegalizer::TetrisLegalizeCluster() {
  /****
   * Legalize the location of all clusters using traditional Tetris legalization algorithm
   * ****/

  // calculate the area ratio
  long int tot_cluster_area = 0;
  long int tot_region_area = (long int) RegionWidth() * (long int) RegionHeight();

  for (auto &cluster: cluster_list_) {
    tot_cluster_area += (long int) cluster.Height() * (long int) cluster.Width();
  }

  double ratio = (double) tot_cluster_area / (double) tot_region_area;
  std::cout << "  Total cluster area: " << tot_cluster_area << "\n";
  std::cout << "  Total region area:  " << tot_region_area << "\n";
  std::cout << "  Ratio: " << ratio << "\n";

  // sort cluster based on the lower left corner
  int tot_cluster_count = cluster_list_.size();
  std::vector<Cluster *> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto &cluster: cluster_list_) {
    cluster_ptr_list[counter] = &cluster;
    ++counter;
  }
  std::sort(cluster_ptr_list.begin(),
            cluster_ptr_list.end(),
            [](const Cluster *lhs, const Cluster *rhs) {
              return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  // initialize the cluster contour of each column
  for (auto &col: clus_cols_) {
    col.contour_ = RegionBottom();
  }
  std::vector<bool> col_full(tot_col_num_, false);

  // find a legal location for each cluster
  int min_cost;
  int min_col;
  int init_x;
  int init_y;
  int res_x;
  int res_y;
  for (auto &cluster_ptr: cluster_ptr_list) {
    min_cost = INT_MAX;
    min_col = 0;
    init_x = cluster_ptr->LLX();
    init_y = cluster_ptr->LLY();
    for (int i = 0; i < tot_col_num_; ++i) {
      int tmp_cost = std::abs(init_x - clus_cols_[i].LLX()) + std::abs(init_y - clus_cols_[i].contour_);
      if (clus_cols_[i].contour_ + cluster_ptr->Height() > RegionTop()) {
        tmp_cost = INT_MAX;
      }
      if (tmp_cost < min_cost) {
        min_cost = tmp_cost;
        min_col = i;
      }
    }
    res_x = clus_cols_[min_col].LLX();
    res_y = clus_cols_[min_col].contour_;
    clus_cols_[min_col].contour_ += cluster_ptr->Height();
    cluster_ptr->ShiftBlock(res_x - init_x, res_y - init_y);
    cluster_ptr->SetLoc(res_x, res_y);
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
  //ClusterBlocks();
  ClusterBlocksLoose();
  //ClusterBlocksCompact();

  TrialClusterLegalization();
  //TetrisLegalizeCluster();

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

  for (auto &cluster: cluster_list_) {
    ost << cluster.LLX() << "\t"
        << cluster.URX() << "\t"
        << cluster.URX() << "\t"
        << cluster.LLX() << "\t"
        << cluster.LLY() << "\t"
        << cluster.LLY() << "\t"
        << cluster.URY() << "\t"
        << cluster.URY() << "\n";
  }
  ost.close();
}
