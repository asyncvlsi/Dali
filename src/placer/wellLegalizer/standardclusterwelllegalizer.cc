//
// Created by Yihang Yang on 3/14/20.
//

#include "standardclusterwelllegalizer.h"

#include <algorithm>

void Cluster::ShiftBlockX(int x_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
  }
}

void Cluster::ShiftBlockY(int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseY(y_disp);
  }
}

void Cluster::ShiftBlock(int x_disp, int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
    blk_ptr->IncreaseY(y_disp);
  }
}

void Cluster::UpdateBlockLocY() {
  //Assert(p_well_height_ + n_well_height_ == height_, "Inconsistency occurs: p_well_height + n_Well_height != height\n");
  for (auto &blk_ptr: blk_list_) {
    auto *well = blk_ptr->Type()->GetWell();
    blk_ptr->SetLLY(ly_ + p_well_height_ - well->GetPWellHeight());
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

void Cluster::LegalizeLooseX() {
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
  int block_contour = lx_;
  int res_x;
  for (auto &blk: blk_list_) {
    res_x = std::max(block_contour, int(blk->LLX()));
    blk->SetLLX(res_x);
    block_contour = int(blk->URX());
  }

  int ux = lx_ + width_;
  if (block_contour > ux) {
    std::sort(blk_list_.begin(),
              blk_list_.end(),
              [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->URX() > blk_ptr1->URX();
              });
    block_contour = ux;
    for (auto &blk: blk_list_) {
      res_x = std::min(block_contour, int(blk->URX()));
      blk->SetURX(res_x);
      block_contour = int(blk->LLX());
    }
  }
}

void Cluster::SetOrient(bool is_orient_N) {
  if (is_orient_N_ != is_orient_N) {
    is_orient_N_ = is_orient_N;
    BlockOrient orient = is_orient_N_ ? N : FS;
    double y_flip_axis = ly_ + height_ / 2.0;
    for (auto &blk_ptr: blk_list_) {
      double ly_to_axis = y_flip_axis - blk_ptr->LLY();
      blk_ptr->SetOrient(orient);
      blk_ptr->SetURY(y_flip_axis + ly_to_axis);
    }
  }
}

void Cluster::InsertWellTapCell(Block &tap_cell) {
  tap_cell_ = &tap_cell;
  blk_list_.emplace_back(tap_cell_);
  tap_cell_->SetCenterX(lx_ + width_ / 2.0);
  auto *well = tap_cell.Type()->GetWell();
  int p_well_height = well->GetPWellHeight();
  int n_well_height = well->GetNWellHeight();
  if (is_orient_N_) {
    tap_cell.SetOrient(N);
    tap_cell.SetLLY(ly_ + p_well_height_ - p_well_height);
  } else {
    tap_cell.SetOrient(FS);
    tap_cell.SetLLY(ly_ + n_well_height_ - n_well_height);
  }
  LegalizeLooseX();
}

void Cluster::UpdateBlockLocationCompact() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->SetLLX(current_x);
    blk->SetCenterY(CenterY());
    current_x += blk->Width();
  }
}

StandardClusterWellLegalizer::StandardClusterWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_cell_width_ = 0;
  max_cluster_width_ = 0;
  col_width_ = 0;
  tot_col_num_ = 0;
}

void StandardClusterWellLegalizer::Init(int cluster_width) {
  // parameters fetching
  auto tech = circuit_->GetTech();
  Assert(tech != nullptr, "No tech info found, well legalization cannot proceed!\n");

  auto n_well_layer = tech->GetNLayer();
  well_spacing_ = std::ceil(n_well_layer->Spacing() / circuit_->GetGridValueX());
  max_unplug_length_ = 3 * (int) std::floor(n_well_layer->MaxPlugDist() / circuit_->GetGridValueX());
  well_tap_cell_width_ = tech->WellTapCell()->Width();

  printf("Well max plug distance: %2.2e um, %d \n", n_well_layer->MaxPlugDist(), max_unplug_length_);
  printf("GridValueX: %2.2e um\n", circuit_->GetGridValueX());
  printf("Well spacing: %2.2e um, %d\n", n_well_layer->Spacing(), well_spacing_);
  printf("Well tap cell width: %d\n", well_tap_cell_width_);

  int max_cell_width = 0;
  for (auto &blk: *BlockList()) {
    if (blk.IsMovable()) {
      max_cell_width = std::max(max_cell_width, blk.Width());
    }
  }
  printf("Max cell width: %d\n", max_cell_width);

  max_cluster_width_ = max_unplug_length_;
  col_width_ = max_unplug_length_ - max_cell_width + well_spacing_;
  if (col_width_ > RegionWidth()) {
    col_width_ = RegionWidth();
  }
  tot_col_num_ = std::ceil(RegionWidth() / (double) col_width_);
  printf("Total number of cluster columns: %d\n", tot_col_num_);

  //std::cout << RegionHeight() << "  " << circuit_->MinHeight() << "\n";
  int max_clusters_per_col = RegionHeight() / circuit_->MinHeight();
  column_list_.resize(tot_col_num_);
  col_width_ = RegionWidth() / tot_col_num_;
  for (int i = 0; i < tot_col_num_; ++i) {
    column_list_[i].lx_ = RegionLeft() + i * col_width_;
    column_list_[i].width_ = col_width_ - well_spacing_;
    column_list_[i].contour_ = RegionBottom();
    column_list_[i].used_height_ = 0;
    column_list_[i].cluster_count_ = 0;
    column_list_[i].max_blk_capacity_per_cluster_ = column_list_[i].Width() / circuit_->MinWidth();
    column_list_[i].cluster_list_.reserve(max_clusters_per_col);
  }
  //column_list_.back().width_ = RegionRight() - column_list_.back().lx_;
  //cluster_list_.reserve(tot_col_num_ * max_clusters_per_col);
  printf("Maximum possible number of clusters in a column: %d\n", max_clusters_per_col);

  well_tap_cell_ = (circuit_->tech_.well_tap_cell_);
  auto *tap_cell_well = well_tap_cell_->GetWell();
  tap_cell_p_height_ = tap_cell_well->GetPWellHeight();
  tap_cell_n_height_ = tap_cell_well->GetNWellHeight();

  index_loc_list_.resize(BlockList()->size());
}

void StandardClusterWellLegalizer::AssignBlockToStrip() {
  // assign blocks to strips
  for (int i = 0; i < tot_col_num_; ++i) {
    column_list_[i].block_count_ = 0;
    column_list_[i].block_list_.clear();
  }
  for (auto &block: *BlockList()) {
    if (block.IsFixed()) continue;
    int col_num = LocToCol((int) std::round(block.X()));
    column_list_[col_num].block_count_++;
  }
  for (int i = 0; i < tot_col_num_; ++i) {
    int capacity = column_list_[i].block_count_;
    column_list_[i].block_list_.reserve(capacity);
  }
  for (auto &block: *BlockList()) {
    if (block.IsFixed()) continue;
    int col_num = LocToCol((int) std::round(block.X()));
    column_list_[col_num].block_list_.push_back(&block);
  }
}

void StandardClusterWellLegalizer::AppendBlockToColBottomUp(ClusterColumn &col, Block &blk) {
  bool is_new_cluster_needed = (col.contour_ == RegionBottom());
  if (!is_new_cluster_needed) {
    bool is_not_in_top_cluster = col.contour_ <= blk.LLY();
    bool is_top_cluster_full = col.front_cluster_->UsedSize() + blk.Width() > col.width_;
    is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.LLY());
  if (col.contour_ != RegionBottom()) {
    init_y = std::max(init_y, col.contour_);
  }

  Cluster *top_cluster;
  auto *blk_well = blk.Type()->GetWell();
  int p_well_height = blk_well->GetPWellHeight();
  int n_well_height = blk_well->GetNWellHeight();
  if (is_new_cluster_needed) {
    col.cluster_list_.emplace_back();
    top_cluster = &(col.cluster_list_.back());
    top_cluster->blk_list_.reserve(col.max_blk_capacity_per_cluster_);
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->SetUsedSize(width + well_tap_cell_width_);
    top_cluster->UpdateHeightFromNPWell(tap_cell_p_height_, tap_cell_n_height_);
    top_cluster->SetLLY(init_y);
    top_cluster->SetLLX(col.LLX());
    top_cluster->SetWidth(col.Width());
    top_cluster->UpdateHeightFromNPWell(p_well_height, n_well_height);

    col.front_cluster_ = top_cluster;
    col.cluster_count_ += 1;
    col.used_height_ += top_cluster->Height();
  } else {
    top_cluster = col.front_cluster_;
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->UseSpace(width);
    if (p_well_height > top_cluster->PHeight() || n_well_height > top_cluster->NHeight()) {
      int old_height = top_cluster->Height();
      top_cluster->UpdateHeightFromNPWell(p_well_height, n_well_height);
      col.used_height_ += top_cluster->Height() - old_height;
    }
  }
  col.contour_ = top_cluster->URY();
}

void StandardClusterWellLegalizer::AppendBlockToColBottomUp(int col_num, Block &blk) {
  auto &col = column_list_[col_num];
  AppendBlockToColBottomUp(col, blk);
}

void StandardClusterWellLegalizer::AppendBlockToColTopDown(ClusterColumn &col, Block &blk) {

}

void StandardClusterWellLegalizer::AppendBlockToColTopDown(int col_num, Block &blk) {

}

void StandardClusterWellLegalizer::AppendBlockToColClose(int col_num, Block &blk) {
  auto &col = column_list_[col_num];
  bool is_new_cluster_needed = (col.contour_ == RegionBottom());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full = col.front_cluster_->UsedSize() + blk.Width() > col.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.LLY());
  if (col.contour_ != RegionBottom()) {
    init_y = std::max(init_y, col.contour_);
  }

  Cluster *top_cluster;
  auto *well = blk.Type()->GetWell();
  int p_well_height = well->GetPWellHeight();
  int n_well_height = well->GetNWellHeight();
  if (is_new_cluster_needed) {
    col.cluster_list_.emplace_back();
    top_cluster = &(col.cluster_list_.back());
    top_cluster->blk_list_.reserve(col.max_blk_capacity_per_cluster_);
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->SetUsedSize(width + well_tap_cell_width_);
    top_cluster->UpdateHeightFromNPWell(tap_cell_p_height_, tap_cell_n_height_);
    top_cluster->SetLLY(init_y);
    top_cluster->SetLLX(col.LLX());
    top_cluster->SetWidth(col.Width());
    top_cluster->UpdateHeightFromNPWell(p_well_height, n_well_height);

    col.front_cluster_ = top_cluster;
    col.cluster_count_ += 1;
    col.used_height_ += top_cluster->Height();
  } else {
    top_cluster = col.front_cluster_;
    top_cluster->blk_list_.push_back(&blk);
    top_cluster->UseSpace(width);
    if (p_well_height > top_cluster->PHeight() || n_well_height > top_cluster->NHeight()) {
      int old_height = top_cluster->Height();
      top_cluster->UpdateHeightFromNPWell(p_well_height, n_well_height);
      col.used_height_ += top_cluster->Height() - old_height;
    }
  }
  col.contour_ = top_cluster->URY();
}

void StandardClusterWellLegalizer::BlockClustering() {
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
    AppendBlockToColBottomUp(col_num, block);
  }

  for (auto &col: column_list_) {
    col.contour_ = RegionBottom();
  }
  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      col.contour_ += cluster.Height();
      cluster.SetLLY(col.contour_);
      cluster.UpdateBlockLocY();
      cluster.LegalizeCompactX(cluster.LLX());
    }
  }
}

void StandardClusterWellLegalizer::BlockClusteringLoose() {
  /****
   * Clustering blocks in each strip
   * ****/
  for (auto &col: column_list_) {
    std::sort(col.block_list_.begin(),
              col.block_list_.end(),
              [](const Block *lhs, const Block *rhs) {
                return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
              });
    for (auto &blk_ptr: col.block_list_) {
      if (blk_ptr->IsFixed()) continue;
      AppendBlockToColBottomUp(col, *blk_ptr);
    }

    if (col.contour_ > RegionTop()) {
      std::cout << "Reverse clustering\n";
      std::sort(col.block_list_.begin(),
                col.block_list_.end(),
                [](const Block *lhs, const Block *rhs) {
                  return (lhs->URY() < rhs->URY()) || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
                });
      for (auto &blk_ptr: col.block_list_) {
        if (blk_ptr->IsFixed()) continue;
        AppendBlockToColTopDown(col, *blk_ptr);
      }
    }

    for (auto &cluster: col.cluster_list_) {
      //cluster.SetLLY(current_ly);
      cluster.UpdateBlockLocY();
      cluster.LegalizeLooseX();
      //current_ly += cluster.Height();
    }
  }
}

void StandardClusterWellLegalizer::BlockClusteringCompact() {
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

  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      /*int col_num = (cluster.LLX() - RegionLeft()) / col_width_;
      auto &col = column_list_[col_num];
      col.contour_ += cluster.Height();
      cluster.SetLLY(col.contour_);*/
      cluster.UpdateBlockLocY();
      cluster.LegalizeLooseX();
    }
  }
}

bool StandardClusterWellLegalizer::TrialClusterLegalization() {
  /****
   * Legalize the location of all clusters using extended Tetris legalization algorithm in columns where usage does not exceed capacity
   * Closely pack the column from bottom to top if its usage exceeds its capacity
   * ****/

  bool res = true;
  std::vector<std::vector<Cluster *>> clusters_in_column(tot_col_num_);
  for (int i = 0; i < tot_col_num_; ++i) {
    clusters_in_column[i].reserve(column_list_[i].cluster_count_);
  }

  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      int col_num = LocToCol((int) std::round(cluster.LLX()));
      clusters_in_column[col_num].emplace_back(&cluster);
    }
  }

  // sort clusters in each column based on the lower left corner
  for (int i = 0; i < tot_col_num_; ++i) {
    auto &col = column_list_[i];
    auto &cluster_list = clusters_in_column[i];
    if (col.contour_ <= RegionTop()) continue;

    //std::cout << "used height/RegionHeight(): " << col.used_height_ / (double) RegionHeight() << "\n";
    if (col.used_height_ <= RegionHeight()) {
      std::sort(cluster_list.begin(),
                cluster_list.end(),
                [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->URY() > rhs->URY());
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
      res = false;
    }
  }

  return res;
}

bool StandardClusterWellLegalizer::TetrisLegalizeCluster() {
  /****
   * Legalize the location of all clusters using traditional Tetris legalization algorithm
   * ****/

  bool is_success = false;
  // calculate the area ratio
  long int tot_cluster_area = 0;
  long int tot_region_area = (long int) RegionWidth() * (long int) RegionHeight();

  int tot_cluster_count = 0;
  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      tot_cluster_area += (long int) cluster.Height() * (long int) cluster.Width();
    }
    tot_cluster_count += col.cluster_list_.size();
  }

  double ratio = (double) tot_cluster_area / (double) tot_region_area;
  std::cout << "  Total cluster area: " << tot_cluster_area << "\n";
  std::cout << "  Total region area:  " << tot_region_area << "\n";
  std::cout << "  Ratio: " << ratio << "\n";

  // sort cluster based on the lower left corner

  std::vector<Cluster *> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      cluster_ptr_list[counter] = &cluster;
      ++counter;
    }
  }
  std::sort(cluster_ptr_list.begin(),
            cluster_ptr_list.end(),
            [](const Cluster *lhs, const Cluster *rhs) {
              return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  // initialize the cluster contour of each column
  for (auto &col: column_list_) {
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
      int tmp_cost = std::abs(init_x - column_list_[i].LLX()) + std::abs(init_y - column_list_[i].contour_);
      if (column_list_[i].contour_ + cluster_ptr->Height() > RegionTop()) {
        tmp_cost = INT_MAX;
      }
      if (tmp_cost < min_cost) {
        min_cost = tmp_cost;
        min_col = i;
      }
    }
    if (min_cost < INT_MAX) {
      res_x = column_list_[min_col].LLX();
      res_y = column_list_[min_col].contour_;
      column_list_[min_col].contour_ += cluster_ptr->Height();
      cluster_ptr->ShiftBlock(res_x - init_x, res_y - init_y);
      cluster_ptr->SetLoc(res_x, res_y);
    } else {
      is_success = false;
    }
  }

  return is_success;
}

double StandardClusterWellLegalizer::WireLengthCost(Cluster *cluster, int l, int r) {
  /****
   * Returns the wire-length cost of the small group from l-th element to r-th element in this cluster
   * "for each order, we keep the left and right boundaries of the group and evenly distribute the cells inside the group.
   * Since we have the Single-Segment Clustering technique to take care of the cell positions,
   * we do not pay much attention to the exact positions of the cells during Local Re-ordering."
   * from "An Efficient and Effective Detailed Placement Algorithm"
   * ****/

  auto &net_list = *NetList();
  std::set<Net *> net_involved;
  for (int i = l; i <= r; ++i) {
    auto *blk = cluster->blk_list_[i];
    for (auto &net_num: blk->net_list) {
      if (net_list[net_num].P() < 100) {
        net_involved.insert(&(net_list[net_num]));
      }
    }
  }

  double res = 0;
  for (auto &net: net_involved) {
    res += net->HPWL();
  }

  return res;
}

void StandardClusterWellLegalizer::FindBestLocalOrder(std::vector<Block *> &res,
                                                      double &cost,
                                                      Cluster *cluster,
                                                      int cur,
                                                      int l,
                                                      int r,
                                                      int left_bound,
                                                      int right_bound,
                                                      int gap,
                                                      int range) {
  /****
  * Returns the best permutation in @param res
  * @param cost records the cost function associated with the best permutation
  * @param l is the left bound of the range
  * @param r is the right bound of the range
  * @param cluster points to the whole range, but we are only interested in the permutation of range [l,r]
  * ****/

  //printf("l : %d, r: %d\n", l, r);

  if (cur == r) {
    cluster->blk_list_[l]->SetLLX(left_bound);
    cluster->blk_list_[r]->SetURX(right_bound);

    int left_contour = left_bound + gap + cluster->blk_list_[l]->Width();
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->blk_list_[i];
      blk->SetLLX(left_contour);
      left_contour += blk->Width() + gap;
    }

    double tmp_cost = WireLengthCost(cluster, l, r);
    if (tmp_cost < cost) {
      cost = tmp_cost;
      for (int j = 0; j < range; ++j) {
        res[j] = cluster->blk_list_[l + j];
      }
    }
  } else {
    // Permutations made
    auto &blk_list = cluster->blk_list_;
    for (int i = cur; i <= r; ++i) {
      // Swapping done
      std::swap(blk_list[cur], blk_list[i]);

      // Recursion called
      FindBestLocalOrder(res, cost, cluster, cur + 1, l, r, left_bound, right_bound, gap, range);

      //backtrack
      std::swap(blk_list[cur], blk_list[i]);
    }
  }

}

void StandardClusterWellLegalizer::LocalReorderInCluster(Cluster *cluster, int range) {
  /****
   * Enumerate all local permutations, @param range determines how big the local range is
   * ****/

  assert(range > 0);

  int sz = cluster->blk_list_.size();
  if (sz < 3) return;

  std::sort(cluster->blk_list_.begin(),
            cluster->blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });

  int last_segment = sz - range;
  std::vector<Block *> res_local_order(range, nullptr);
  for (int l = 0; l <= last_segment; ++l) {
    int tot_blk_width = 0;
    for (int j = 0; j < range; ++j) {
      res_local_order[j] = cluster->blk_list_[l + j];
      tot_blk_width += res_local_order[j]->Width();
    }
    int r = l + range - 1;
    double best_cost = DBL_MAX;
    int left_bound = (int) cluster->blk_list_[l]->LLX();
    int right_bound = (int) cluster->blk_list_[r]->URX();
    int gap = (right_bound - left_bound - tot_blk_width) / (r - l);

    FindBestLocalOrder(res_local_order, best_cost, cluster, l, l, r, left_bound, right_bound, gap, range);
    for (int j = 0; j < range; ++j) {
      cluster->blk_list_[l + j] = res_local_order[j];
    }

    cluster->blk_list_[l]->SetLLX(left_bound);
    cluster->blk_list_[r]->SetURX(right_bound);
    int left_contour = left_bound + cluster->blk_list_[l]->Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->blk_list_[i];
      blk->SetLLX(left_contour);
      left_contour += blk->Width() + gap;
    }
  }

}

void StandardClusterWellLegalizer::LocalReorderAllClusters() {
  // sort cluster based on the lower left corner
  int tot_cluster_count = 0;
  for (auto &col: column_list_) {
    tot_cluster_count += col.cluster_list_.size();
  }
  std::vector<Cluster *> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      cluster_ptr_list[counter] = &cluster;
      ++counter;
    }
  }
  std::sort(cluster_ptr_list.begin(),
            cluster_ptr_list.end(),
            [](const Cluster *lhs, const Cluster *rhs) {
              return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  for (auto &cluster_ptr: cluster_ptr_list) {
    LocalReorderInCluster(cluster_ptr, 3);
  }
}

void StandardClusterWellLegalizer::SingleSegmentClusteringOptimization() {

}

void StandardClusterWellLegalizer::UpdateClusterOrient() {
  for (auto &col: column_list_) {
    bool is_orient_N = is_first_row_orient_N_;
    for (auto &clus_ptr: col.cluster_list_) {
      clus_ptr.SetOrient(is_orient_N);
      is_orient_N = !is_orient_N;
    }
  }
}

void StandardClusterWellLegalizer::InsertWellTapAroundMiddle() {
  auto &tap_cell_list = circuit_->design_.well_tap_list;
  tap_cell_list.clear();
  int tot_cluster_count = 0;
  for (auto &col: column_list_) {
    tot_cluster_count += col.cluster_list_.size();
  }
  tap_cell_list.reserve(tot_cluster_count);
  circuit_->design_.tap_name_map.clear();

  int counter = 0;
  for (auto &col: column_list_) {
    for (auto &cluster: col.cluster_list_) {
      std::string block_name = "__well_tap__" + std::to_string(counter++);
      tap_cell_list.emplace_back();
      auto &tap_cell = tap_cell_list.back();
      tap_cell.SetType(circuit_->tech_.WellTapCell());
      int map_size = circuit_->design_.tap_name_map.size();
      auto ret = circuit_->design_.tap_name_map.insert(std::pair<std::string, int>(block_name, map_size));
      auto *name_num_pair_ptr = &(*ret.first);
      tap_cell.SetNameNumPair(name_num_pair_ptr);
      cluster.InsertWellTapCell(tap_cell);
    }
  }
}

void StandardClusterWellLegalizer::ClearCachedData() {
  for (auto &block: *BlockList()) {
    block.SetOrient(N);
  }

  for (int i = 0; i < tot_col_num_; ++i) {
    column_list_[i].contour_ = RegionBottom();
    column_list_[i].used_height_ = 0;
    column_list_[i].cluster_count_ = 0;

    column_list_[i].cluster_list_.clear();
    column_list_[i].front_cluster_ = nullptr;
  }

  //cluster_list_.clear();
}

bool StandardClusterWellLegalizer::WellLegalize() {
  ClearCachedData();
  bool is_success;

  BlockClusteringLoose();
  //BlockClusteringCompact();
  ReportHPWL(LOG_CRITICAL);

  is_success = TrialClusterLegalization();
  ReportHPWL(LOG_CRITICAL);
  /*if (!is_success) {
    is_success = TetrisLegalizeCluster();
    ReportHPWL(LOG_CRITICAL);
  }*/

  //UpdateClusterInColumn();
  UpdateClusterOrient();
  for (int i = 0; i < 6; ++i) {
    LocalReorderAllClusters();
    ReportHPWL(LOG_CRITICAL);
  }

  InsertWellTapAroundMiddle();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    if (is_success) {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization complete!\n"
                << "\033[0m";
    } else {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization fail!\n"
                << "\033[0m";
    }
  }

  return is_success;
}

bool StandardClusterWellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Standard Cluster Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  /****---->****/
  bool is_success = true;
  Init();
  AssignBlockToStrip();
  //BlockClustering();
  BlockClusteringLoose();
  //BlockClusteringCompact();
  ReportHPWL(LOG_CRITICAL);

  is_success = TrialClusterLegalization();
  ReportHPWL(LOG_CRITICAL);
  /*if (!is_success) {
    is_success = TetrisLegalizeCluster();
    ReportHPWL(LOG_CRITICAL);
  }*/

  //UpdateClusterInColumn();
  UpdateClusterOrient();
  for (int i = 0; i < 6; ++i) {
    LocalReorderAllClusters();
    ReportHPWL(LOG_CRITICAL);
  }

  InsertWellTapAroundMiddle();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    if (is_success) {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization complete!\n"
                << "\033[0m";
    } else {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization fail!\n"
                << "\033[0m";
    }
  }
  ReportHPWL(LOG_CRITICAL);
  /****<----****/

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("(wall time: %.4fs, cpu time: %.4fs)\n", wall_time, cpu_time);
  }

  ReportMemory(LOG_CRITICAL);
  GenMatlabClusterTable("sc_result");

  return is_success;
}

void StandardClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);

  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &col:column_list_) {
    for (auto &cluster: col.cluster_list_) {
      ost << cluster.LLX() << "\t"
          << cluster.URX() << "\t"
          << cluster.URX() << "\t"
          << cluster.LLX() << "\t"
          << cluster.LLY() << "\t"
          << cluster.LLY() << "\t"
          << cluster.URY() << "\t"
          << cluster.URY() << "\n";
    }
  }
  ost.close();
}

void StandardClusterWellLegalizer::GenMATLABWellTable(std::string const &name_of_file) {
  circuit_->GenMATLABWellTable(name_of_file);

  std::string p_file = name_of_file + "_pwell.txt";
  std::ofstream ostp(p_file.c_str());
  Assert(ostp.is_open(), "Cannot open output file: " + p_file);

  std::string n_file = name_of_file + "_nwell.txt";
  std::ofstream ostn(n_file.c_str());
  Assert(ostn.is_open(), "Cannot open output file: " + n_file);

  for (auto &col: column_list_) {
    std::vector<int> pn_edge_list;
    pn_edge_list.reserve(col.cluster_list_.size() + 2);
    pn_edge_list.push_back(RegionBottom());
    for (auto &cluster: col.cluster_list_) {
      pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
    }
    pn_edge_list.push_back(RegionTop());

    bool is_p_well_rect = col.is_first_row_orient_N_;
    int lx = col.LLX();
    int ux = col.URX();
    int ly;
    int uy;
    int rect_count = (int) pn_edge_list.size() - 1;
    for (int i = 0; i < rect_count; ++i) {
      ly = pn_edge_list[i];
      uy = pn_edge_list[i + 1];
      if (is_p_well_rect) {
        ostp << lx << "\t"
             << ux << "\t"
             << ux << "\t"
             << lx << "\t"
             << ly << "\t"
             << ly << "\t"
             << uy << "\t"
             << uy << "\n";
      } else {
        ostn << lx << "\t"
             << ux << "\t"
             << ux << "\t"
             << lx << "\t"
             << ly << "\t"
             << ly << "\t"
             << uy << "\t"
             << uy << "\n";
      }
      is_p_well_rect = !is_p_well_rect;
    }
  }
  ostp.close();
  ostn.close();
}

void StandardClusterWellLegalizer::EmitDEFWellFile(std::string const &name_of_file, std::string const &input_def_file) {
  /****
   * Emit three files:
   * 1. def file, well tap cells are included in this DEF file
   * 2. rect file including all N/P well rectangles
   * 3. cluster file including all cluster shapes
   * ****/

  // emit def file
  circuit_->SaveDefFile(name_of_file + ".def", input_def_file);

  double factor_x = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_x_;
  double factor_y = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_y_;
  // emit rect file
  std::string rect_file_name = name_of_file + "_well.rect";
  std::ofstream ost(rect_file_name.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + rect_file_name);

  for (auto &col: column_list_) {
    std::vector<int> pn_edge_list;
    pn_edge_list.reserve(col.cluster_list_.size() + 2);
    pn_edge_list.push_back(RegionBottom());
    for (auto &cluster: col.cluster_list_) {
      pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
    }
    pn_edge_list.push_back(RegionTop());

    bool is_p_well_rect = col.is_first_row_orient_N_;
    int lx = col.LLX();
    int ux = col.URX();
    int ly;
    int uy;
    int rect_count = (int) pn_edge_list.size() - 1;
    for (int i = 0; i < rect_count; ++i) {
      ly = pn_edge_list[i];
      uy = pn_edge_list[i + 1];
      if (is_p_well_rect) {
        ost << "pwell GND ";
      } else {
        ost << "nwell Vdd ";
      }
      ost << (int) (lx * factor_x) + circuit_->design_.die_area_offset_x << " "
          << (int) (ly * factor_y) + circuit_->design_.die_area_offset_y << " "
          << (int) (ux * factor_x) + circuit_->design_.die_area_offset_x << " "
          << (int) (uy * factor_y) + circuit_->design_.die_area_offset_y << "\n";
      is_p_well_rect = !is_p_well_rect;
    }
  }
  ost.close();

  // emit cluster file
  std::string cluster_file_name = name_of_file + "_router.cluster";
  std::ofstream ost1(cluster_file_name.c_str());
  Assert(ost1.is_open(), "Cannot open output file: " + cluster_file_name);

  for (int i = 0; i < tot_col_num_; ++i) {
    std::string column_name = "column" + std::to_string(i);
    ost1 << "STRIP " << column_name << "\n";

    auto &col = column_list_[i];
    ost1 << "  "
         << (int) (col.LLX() * factor_x) + circuit_->design_.die_area_offset_x << "  "
         << (int) (col.URX() * factor_x) + circuit_->design_.die_area_offset_x << "  ";
    if (col.is_first_row_orient_N_) {
      ost1 << "GND\n";
    } else {
      ost1 << "Vdd\n";
    }

    for (auto &cluster: col.cluster_list_) {
      ost1 << "  "
           << (int) (cluster.LLY() * factor_y) + circuit_->design_.die_area_offset_y << "  "
           << (int) (cluster.URY() * factor_y) + circuit_->design_.die_area_offset_y << "\n";
    }

    ost1 << "END " << column_name << "\n\n";
  }
  ost1.close();
}
