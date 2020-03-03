//
// Created by Yihang Yang on 2/28/20.
//

#include "clusterwelllegalizer.h"

#include <algorithm>

BlkCluster::BlkCluster(int well_extension_x_init, int well_extension_y_init, int plug_width_init) :
    well_extension_x_(well_extension_x_init),
    well_extension_y_(well_extension_y_init),
    plug_width_(plug_width_init) {}

void BlkCluster::AppendBlock(Block &block) {
  if (blk_ptr_list_.empty()) {
    lx_ = int(block.LLX()) - well_extension_x_;
    modified_lx_ = lx_ - well_extension_x_ - plug_width_;
    ly_ = int(block.LLY()) - well_extension_y_;
    width_ = block.Width() + well_extension_x_ * 2 + plug_width_;
    height_ = block.Height() + well_extension_y_ * 2;
  } else {
    width_ += block.Width();
    if (block.Height() > height_) {
      ly_ -= (block.Height() - height_ + 1) / 2;
      height_ = block.Height() + well_extension_y_ * 2;
    }
  }
  blk_ptr_list_.push_back(&block);
}

void BlkCluster::OptimizeHeight() {
  /****
   * This function aligns all N/P well boundaries of cells inside a cluster
   * ****/

}

void BlkCluster::UpdateBlockLocation() {
  int current_loc = lx_;
  for (auto &blk_ptr: blk_ptr_list_) {
    blk_ptr->SetLLX(current_loc);
    blk_ptr->SetCenterY(this->CenterY());
    current_loc += blk_ptr->Width();
  }
}

ClusterWellLegalizer::ClusterWellLegalizer() : LGTetrisEx() {}

ClusterWellLegalizer::~ClusterWellLegalizer() {
  for (auto &cluster_ptr: cluster_set_) {
    delete cluster_ptr;
  }
}

void ClusterWellLegalizer::InitializeClusterLegalizer() {
  InitLegalizer();
  row_to_cluster_.resize(tot_num_rows_, nullptr);
  auto tech_params = circuit_->GetTech();
  Assert(tech_params != nullptr, "No tech info found, well legalization cannot proceed!\n");
  auto n_well_layer = tech_params->GetNLayer();
  auto p_well_layer = tech_params->GetPLayer();
  well_extension_x = std::ceil(n_well_layer->Overhang() / circuit_->GetGridValueX());
  //well_extension_y = std::ceil((n_well_layer->Overhang())/circuit_->GetGridValueY());
  //plug_width = std::ceil();
  printf("Well max plug distance: %2.2e um \n", n_well_layer->MaxPlugDist());
  printf("GridValueX: %2.2e um\n", circuit_->GetGridValueX());
  max_well_length = std::floor(n_well_layer->MaxPlugDist() / circuit_->GetGridValueX());

  if (max_well_length > RegionWidth()) {
    max_well_length = RegionWidth();
  } else if (max_well_length >= RegionWidth() / 2) {
    max_well_length = RegionWidth() / 2;
  } else if (max_well_length >= RegionWidth() / 3) {
    max_well_length = RegionWidth() / 3;
  }

  new_cluster_cost_threshold = circuit_->MinHeight() * 5;
}

BlkCluster *ClusterWellLegalizer::CreateNewCluster() {
  auto *new_cluster = new BlkCluster(well_extension_x, well_extension_y, plug_width);
  cluster_set_.insert(new_cluster);
  return new_cluster;
}

void ClusterWellLegalizer::AddBlockToCluster(Block &block, BlkCluster *cluster) {
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

BlkCluster *ClusterWellLegalizer::FindClusterForBlock(Block &block) {
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

  BlkCluster *res_cluster = nullptr;
  BlkCluster *pre_cluster = nullptr;
  BlkCluster *cur_cluster = nullptr;

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

void ClusterWellLegalizer::ClusterBlocks() {
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

    BlkCluster *cluster = FindClusterForBlock(block);
    assert(cluster != nullptr);
    AddBlockToCluster(block, cluster);
  }
}

void ClusterWellLegalizer::UseSpaceLeft(int end_x, int lo_row, int hi_row) {
  assert(lo_row >= 0);
  assert(hi_row < tot_num_rows_);
  for (int i = lo_row; i <= hi_row; ++i) {
    block_contour_[i] = end_x;
  }
}

bool ClusterWellLegalizer::LegalizeClusterLeft() {
  bool is_successful = true;
  block_contour_.assign(block_contour_.size(), left_);

  int i = 0;
  for (auto &cluster_ptr: cluster_set_) {
    cluster_loc_list_[i].clus_ptr = cluster_ptr;
    cluster_loc_list_[i].x = cluster_ptr->LLX();
    cluster_loc_list_[i].y = cluster_ptr->LLY();
    //std::cout << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
    ++i;
  }
  std::sort(cluster_loc_list_.begin(), cluster_loc_list_.end());

  int height;
  int width;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_legal_loc_found;
  int sz = cluster_loc_list_.size();

  for (i = 0; i < sz; ++i) {
    //std::cout << i << "\n";
    auto cluster = cluster_loc_list_[i].clus_ptr;
    assert(cluster != nullptr);

    res.x = int(std::round(cluster->LLX()));
    res.y = AlignedLocToRowLoc(cluster->LLY());
    height = int(cluster->Height());
    width = int(cluster->Width());

    is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocLeft(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //std::cout << res.x << "  " << res.y << "  " << cluster.Num() << " left\n";
        //break;
      }
    }

    cluster->SetLoc(res.x, res.y);

    UseSpaceLeft(cluster->URX(), StartRow(cluster->LLY()), EndRow(cluster->URY()));
  }

  return is_successful;
}

void ClusterWellLegalizer::UseSpaceRight(int end_x, int lo_row, int hi_row) {
  assert(lo_row >= 0);
  assert(hi_row < tot_num_rows_);
  for (int i = lo_row; i <= hi_row; ++i) {
    block_contour_[i] = end_x;
  }
}

bool ClusterWellLegalizer::LegalizeClusterRight() {
  bool is_successful = true;
  block_contour_.assign(block_contour_.size(), right_);

  int sz = cluster_loc_list_.size();
  int i = 0;
  for (auto &cluster_ptr: cluster_set_) {
    cluster_loc_list_[i].clus_ptr = cluster_ptr;
    cluster_loc_list_[i].x = cluster_ptr->LLX();
    cluster_loc_list_[i].y = cluster_ptr->LLY();
    //std::cout << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
    ++i;
  }
  std::sort(cluster_loc_list_.begin(),
            cluster_loc_list_.end(),
            [](const CluPtrLocPair &lhs, const CluPtrLocPair &rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });

  int height;
  int width;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_legal_loc_found;

  for (i = 0; i < sz; ++i) {
    //std::cout << i << "\n";
    auto cluster = cluster_loc_list_[i].clus_ptr;
    assert(cluster != nullptr);

    res.x = int(std::round(cluster->URX()));
    res.y = AlignedLocToRowLoc(cluster->LLY());
    height = int(cluster->Height());
    width = int(cluster->Width());

    is_current_loc_legal = IsCurrentLocLegalRight(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocRight(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //std::cout << res.x << "  " << res.y << "  " << cluster.Num() << " left\n";
        //break;
      }
    }

    cluster->SetURX(res.x);
    cluster->SetLLY(res.y);

    UseSpaceRight(cluster->LLX(), StartRow(cluster->LLY()), EndRow(cluster->URY()));
  }

  return is_successful;
}

bool ClusterWellLegalizer::LegalizeCluster() {
  CluPtrLocPair tmp_clu_ptr_pair(nullptr, 0, 0);
  cluster_loc_list_.assign(cluster_set_.size(), tmp_clu_ptr_pair);

  long int tot_cluster_area = 0;
  for (auto &cluster: cluster_set_) {
    tot_cluster_area += cluster->Area();
  }

  std::cout << "Total cluster area: " << tot_cluster_area << "\n";
  std::cout << "Total region area : " << RegionHeight() * RegionWidth() << "\n";
  std::cout << "            Ratio : " << double(tot_cluster_area) / RegionWidth() / RegionHeight() << "\n";

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (legalize_from_left_) {
      is_success = LegalizeClusterLeft();
    } else {
      is_success = LegalizeClusterRight();
    }
    std::cout << cur_iter_ << "-th iteration: " << is_success << "\n";
    UpdateBlockLocation();
    legalize_from_left_ = !legalize_from_left_;
    ++k_left_;
    GenMatlabClusterTable("cl" + std::to_string(cur_iter_) + "_result");
    ReportHPWL(LOG_CRITICAL);
    if (is_success) {
      break;
    }
  }
  if (!is_success) {
    std::cout << "Legalization fails\n";
  }
  return is_success;
}

void ClusterWellLegalizer::UpdateBlockLocation() {
  for (auto &cluster_ptr : cluster_set_) {
    cluster_ptr->UpdateBlockLocation();
  }
}

void ClusterWellLegalizer::BlockGlobalSwap() {

}

void ClusterWellLegalizer::BlockVerticalSwap() {

}

void ClusterWellLegalizer::LocalReorderInCluster(BlkCluster *cluster, int range) {

}

void ClusterWellLegalizer::LocalReorderAllClusters() {
  int i = 0;
  for (auto &cluster_ptr: cluster_set_) {
    cluster_loc_list_[i].clus_ptr = cluster_ptr;
    cluster_loc_list_[i].x = cluster_ptr->LLX();
    cluster_loc_list_[i].y = cluster_ptr->LLY();
    //std::cout << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
    ++i;
  }
  std::sort(cluster_loc_list_.begin(), cluster_loc_list_.end());

  for (auto &pair: cluster_loc_list_) {
    LocalReorderInCluster(pair.clus_ptr);
  }
}

void ClusterWellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  /****---->****/

  InitializeClusterLegalizer();
  ReportWellRule();
  ClusterBlocks();
  LegalizeCluster();
  UpdateBlockLocation();

  /****<----****/

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Cluster Well Legalization complete!\n"
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

  GenMatlabClusterTable("cl_result");
}

void ClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);

  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &cluster: cluster_set_) {
    ost << cluster->LLX() << "\t"
        << cluster->URX() << "\t"
        << cluster->URX() << "\t"
        << cluster->LLX() << "\t"
        << cluster->LLY() << "\t"
        << cluster->LLY() << "\t"
        << cluster->URY() << "\t"
        << cluster->URY() << "\n";
  }
  ost.close();
}

void ClusterWellLegalizer::ReportWellRule() {
  std::cout << "  Number of rows: " << tot_num_rows_ << "\n"
            << "  Number of blocks: " << index_loc_list_.size() << "\n"
            << "  Well Rules:\n"
            << "    WellSpacing: " << well_spacing_x << "\n"
            << "    MaxDist:     " << max_well_length << "\n"
            << "    (real):      " << std::floor(circuit_->GetTech()->GetNLayer()->MaxPlugDist() / circuit_->GetGridValueX()) << "\n"
            << "    WellWidth:   " << well_min_width << "\n"
            << "    OverhangX:   " << well_extension_x << "\n"
            << "    OverhangY:   " << well_extension_y << "\n";
}
