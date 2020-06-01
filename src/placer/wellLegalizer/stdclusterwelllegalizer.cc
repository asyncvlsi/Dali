//
// Created by Yihang Yang on 3/14/20.
//

#include "stdclusterwelllegalizer.h"

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
    blk_ptr->setLLY(ly_ + p_well_height_ - well->GetPWellHeight());
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
    blk->setLLX(current_x);
    current_x += blk->Width();
  }
}

void Cluster::LegalizeCompactX() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->setLLX(current_x);
    current_x += blk->Width();
  }
}

void Cluster::LegalizeLooseX(int space_to_well_tap) {
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
    blk->setLLX(res_x);
    block_contour = int(blk->URX());
    if ((tap_cell_ != nullptr) && (blk->Type() == tap_cell_->Type())) {
      block_contour += space_to_well_tap;
    }
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
      blk->setURX(res_x);
      block_contour = int(blk->LLX());
      if ((tap_cell_ != nullptr) && (blk->Type() == tap_cell_->Type())) {
        block_contour -= space_to_well_tap;
      }
    }
  }
}

void Cluster::SetOrient(bool is_orient_N) {
  if (is_orient_N_ != is_orient_N) {
    is_orient_N_ = is_orient_N;
    BlockOrient orient = is_orient_N_ ? N_ : FS_;
    double y_flip_axis = ly_ + height_ / 2.0;
    for (auto &blk_ptr: blk_list_) {
      double ly_to_axis = y_flip_axis - blk_ptr->LLY();
      blk_ptr->setOrient(orient);
      blk_ptr->setURY(y_flip_axis + ly_to_axis);
    }
  }
}

void Cluster::InsertWellTapCell(Block &tap_cell, int loc) {
  tap_cell_ = &tap_cell;
  blk_list_.emplace_back(tap_cell_);
  tap_cell_->setCenterX(loc);
  auto *well = tap_cell.Type()->GetWell();
  int p_well_height = well->GetPWellHeight();
  int n_well_height = well->GetNWellHeight();
  if (is_orient_N_) {
    tap_cell.setOrient(N_);
    tap_cell.setLLY(ly_ + p_well_height_ - p_well_height);
  } else {
    tap_cell.setOrient(FS_);
    tap_cell.setLLY(ly_ + n_well_height_ - n_well_height);
  }
}

void Cluster::UpdateBlockLocationCompact() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->setLLX(current_x);
    blk->setCenterY(CenterY());
    current_x += blk->Width();
  }
}

Strip *ClusterStrip::GetStripMatchSeg(SegI seg, int y_loc) {
  Strip *res = nullptr;
  for (auto &&strip: strip_list_) {
    if ((strip.URY() == y_loc) && (strip.LLX() == seg.lo) && (strip.URX() == seg.hi)) {
      res = &strip;
      break;
    }
  }
  return res;
}

Strip *ClusterStrip::GetStripMatchBlk(Block *blk_ptr) {
  Strip *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  for (auto &&strip: strip_list_) {
    if ((strip.LLY() <= center_y) &&
        (strip.URY() > center_y) &&
        (strip.LLX() <= center_x) &&
        (strip.URX() > center_x)) {
      res = &strip;
      break;
    }
  }
  return res;
}

Strip *ClusterStrip::GetStripClosestToBlk(Block *blk_ptr, double &distance) {
  Strip *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto &&strip: strip_list_) {
    double tmp_distance;
    if ((strip.LLY() <= center_y) &&
        (strip.URY() > center_y) &&
        (strip.LLX() <= center_x) &&
        (strip.URX() > center_x)) {
      res = &strip;
      tmp_distance = 0;
    } else if ((strip.LLX() <= center_x) && (strip.URX() > center_x)) {
      tmp_distance = std::min(std::abs(center_y - strip.LLY()), std::abs(center_y - strip.URY()));
    } else if ((strip.LLY() <= center_y) && (strip.URY() > center_y)) {
      tmp_distance = std::min(std::abs(center_x - strip.LLX()), std::abs(center_x - strip.URX()));
    } else {
      tmp_distance = std::min(std::abs(center_x - strip.LLX()), std::abs(center_x - strip.URX()))
          + std::min(std::abs(center_y - strip.LLY()), std::abs(center_y - strip.URY()));
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &strip;
    }
  }

  distance = min_distance;
  return res;
}

void ClusterStrip::AssignBlockToSimpleStrip() {
  for (auto &strip: strip_list_) {
    strip.block_count_ = 0;
    strip.block_list_.clear();
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto strip = GetStripClosestToBlk(blk_ptr, tmp_dist);
    strip->block_count_++;
  }

  for (auto &strip: strip_list_) {
    strip.block_list_.reserve(strip.block_count_);
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto strip = GetStripClosestToBlk(blk_ptr, tmp_dist);
    strip->block_list_.push_back(blk_ptr);
  }
}

StdClusterWellLegalizer::StdClusterWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_cell_width_ = 0;
  strip_width_ = 0;
  tot_col_num_ = 0;
  row_height_set_ = false;
}

void StdClusterWellLegalizer::InitAvailSpace() {
  if (!row_height_set_) {
    row_height_ = circuit_->GetIntRowHeight();
  }
  tot_num_rows_ = (top_ - bottom_) / row_height_;

  std::vector<std::vector<std::vector<int>>> macro_segments;
  macro_segments.resize(tot_num_rows_);
  std::vector<int> tmp(2, 0);
  bool out_of_range;
  for (auto &block: *BlockList()) {
    if (block.IsMovable()) continue;
    int ly = int(std::floor(block.LLY()));
    int uy = int(std::ceil(block.URY()));
    int lx = int(std::floor(block.LLX()));
    int ux = int(std::ceil(block.URX()));

    out_of_range = (ly >= RegionTop()) || (uy <= RegionBottom()) || (lx >= RegionRight()) || (ux <= RegionLeft());

    if (out_of_range) continue;

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    tmp[0] = std::max(RegionLeft(), lx);
    tmp[1] = std::min(RegionRight(), ux);
    if (tmp[1] > tmp[0]) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto &intervals : macro_segments) {
    LGTetrisEx::MergeIntervals(intervals);
  }

  std::vector<std::vector<int>> intermediate_seg_rows;
  intermediate_seg_rows.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    if (macro_segments[i].empty()) {
      intermediate_seg_rows[i].push_back(left_);
      intermediate_seg_rows[i].push_back(right_);
      continue;
    }
    int segments_size = int(macro_segments[i].size());
    for (int j = 0; j < segments_size; ++j) {
      auto &interval = macro_segments[i][j];
      if (interval[0] == left_ && interval[1] < RegionRight()) {
        intermediate_seg_rows[i].push_back(interval[1]);
      }

      if (interval[0] > left_) {
        if (intermediate_seg_rows[i].empty()) {
          intermediate_seg_rows[i].push_back(left_);
        }
        intermediate_seg_rows[i].push_back(interval[0]);
        if (interval[1] < RegionRight()) {
          intermediate_seg_rows[i].push_back(interval[1]);
        }
      }
    }
    if (intermediate_seg_rows[i].size() % 2 == 1) {
      intermediate_seg_rows[i].push_back(right_);
    }
  }

  white_space_in_rows_.resize(tot_num_rows_);
  int min_blk_width = int(circuit_->MinBlkWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    white_space_in_rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j] >= min_blk_width) {
        white_space_in_rows_[i].emplace_back(intermediate_seg_rows[i][j], intermediate_seg_rows[i][j + 1]);
      }
    }
  }

  //GenAvailSpace();
}

void StdClusterWellLegalizer::FetchNPWellParams() {
  auto tech = circuit_->GetTech();
  Assert(tech != nullptr, "No tech info found, well legalization cannot proceed!\n");

  auto n_well_layer = tech->GetNLayer();
  int same_well_spacing = std::ceil(n_well_layer->Spacing() / circuit_->GetGridValueX());
  int op_well_spacing = std::ceil(n_well_layer->OpSpacing() / circuit_->GetGridValueX());
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ = (int) std::floor(n_well_layer->MaxPlugDist() / circuit_->GetGridValueX());
  well_tap_cell_ = tech->WellTapCell();
  Assert(well_tap_cell_ != nullptr, "Cannot find the definition of well tap cell, well legalization cannot proceed\n");
  well_tap_cell_width_ = tech->WellTapCell()->Width();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Well max plug distance: %2.2e um, %d \n", n_well_layer->MaxPlugDist(), max_unplug_length_);
    printf("GridValueX: %2.2e um\n", circuit_->GetGridValueX());
    printf("Well spacing: %2.2e um, %d\n", n_well_layer->Spacing(), well_spacing_);
    printf("Well tap cell width: %d\n", well_tap_cell_width_);
  }

  well_tap_cell_ = (circuit_->tech_.well_tap_cell_ptr_);
  auto *tap_cell_well = well_tap_cell_->GetWell();
  tap_cell_p_height_ = tap_cell_well->GetPWellHeight();
  tap_cell_n_height_ = tap_cell_well->GetNWellHeight();
}

void StdClusterWellLegalizer::UpdateWhiteSpaceInCol(ClusterStrip &col) {
  SegI strip_seg(col.LLX(), col.URX());
  col.white_space_.clear();

  col.white_space_.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    for (auto &seg: white_space_in_rows_[i]) {
      auto tmp_seg = strip_seg.Joint(seg);
      if (tmp_seg != nullptr) {
        if (tmp_seg->lo - seg.lo < max_cell_width_ * 2 + well_spacing_) {
          if (tmp_seg->hi - seg.lo < 2 * max_unplug_length_) {
            tmp_seg->lo = seg.lo;
          }
        }
        if (seg.hi - tmp_seg->hi < max_cell_width_ * 2 + well_spacing_) {
          if (seg.hi - tmp_seg->lo < 2 * max_unplug_length_) {
            tmp_seg->hi = seg.hi;
          }
        }
        if (tmp_seg->Span() < max_cell_width_ * 2 && tmp_seg->Span() < seg.Span()) continue;;
        col.white_space_[i].push_back(*tmp_seg);
      }
      delete tmp_seg;
    }
  }
}

void StdClusterWellLegalizer::DecomposeToSimpleStrip() {
  for (auto &col: col_list_) {
    for (int i = 0; i < tot_num_rows_; ++i) {
      for (auto &seg: col.white_space_[i]) {
        int y_loc = RowToLoc(i);
        auto strip = col.GetStripMatchSeg(seg, y_loc);
        if (strip == nullptr) {
          col.strip_list_.emplace_back();
          strip = &(col.strip_list_.back());
          strip->lx_ = seg.lo;
          strip->width_ = seg.Span();
          strip->ly_ = y_loc;
          strip->height_ = row_height_;
          strip->contour_ = y_loc;
          strip->front_cluster_ = nullptr;
          strip->used_height_ = 0;
          strip->max_blk_capacity_per_cluster_ = strip->width_ / circuit_->MinBlkWidth();
        } else {
          strip->height_ += row_height_;
        }
      }
    }
  }

  col_list_[tot_col_num_ - 1].strip_list_[0].width_ =
      RegionRight() - col_list_[tot_col_num_ - 1].strip_list_[0].LLX() - well_spacing_;
  col_list_[tot_col_num_ - 1].strip_list_[0].max_blk_capacity_per_cluster_ =
      col_list_[tot_col_num_ - 1].strip_list_[0].width_ / circuit_->MinBlkWidth();
  /*for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      strip.height_ -= row_height_;
    }
  }*/

  //GenSimpleStrips();
}

void StdClusterWellLegalizer::Init(int cluster_width) {
  // fetch parameters about N/P-well
  FetchNPWellParams();
  space_to_well_tap_ = 0;

  // temporarily change left and right boundary to reserve space
  //printf("left: %d, right: %d, width: %d\n", left_, right_, RegionWidth());
  left_ += well_spacing_;
  right_ -= well_spacing_;
  //printf("left: %d, right: %d, width: %d\n", left_, right_, RegionWidth());

  // initialize row height and white space segments
  InitAvailSpace();

  max_cell_width_ = 0;
  for (auto &blk: *BlockList()) {
    if (blk.IsMovable()) {
      max_cell_width_ = std::max(max_cell_width_, blk.Width());
    }
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Max cell width: %d\n", max_cell_width_);
  }

  if (cluster_width <= 0) {
    if (globalVerboseLevel >= LOG_CRITICAL) {
      std::cout << "Using default cluster width: 2*max_unplug_length_\n";
    }
    strip_width_ = max_unplug_length_ * 2;
  } else {
    if (cluster_width < max_unplug_length_) {
      if (globalVerboseLevel >= LOG_WARNING) {
        std::cout << "WARNING:\n"
                  << "  Specified cluster width is smaller than max_unplug_length_, "
                  << "  space is wasted, may not be able to successfully complete well legalization\n";
      }
    }
    strip_width_ = cluster_width;
  }

  strip_width_ = strip_width_ + well_spacing_;
  if (strip_width_ > RegionWidth()) {
    strip_width_ = RegionWidth();
  }
  tot_col_num_ = std::ceil(RegionWidth() / (double) strip_width_);
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Total number of cluster columns: %d\n", tot_col_num_);
  }

  //std::cout << RegionHeight() << "  " << circuit_->MinBlkHeight() << "\n";
  int max_clusters_per_col = RegionHeight() / circuit_->MinBlkHeight();
  col_list_.resize(tot_col_num_);
  strip_width_ = RegionWidth() / tot_col_num_;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Cluster width: %2.2e um\n", strip_width_ * circuit_->tech_.grid_value_x_);
  }
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list_[i].lx_ = RegionLeft() + i * strip_width_;
    col_list_[i].width_ = strip_width_ - well_spacing_;
    UpdateWhiteSpaceInCol(col_list_[i]);
  }
  DecomposeToSimpleStrip();
  //GenAvailSpaceInCols();
  //col_list_.back().width_ = RegionURX() - col_list_.back().lx_;
  //cluster_list_.reserve(tot_col_num_ * max_clusters_per_col);

  // restore left and right boundaries back
  left_ -= well_spacing_;
  right_ += well_spacing_;
  //printf("left: %d, right: %d\n", left_, right_);
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Maximum possible number of clusters in a column: %d\n", max_clusters_per_col);
  }

  index_loc_list_.resize(BlockList()->size());
  // temporarily change left and right boundary to reserve space

}

void StdClusterWellLegalizer::AssignBlockToColBasedOnWhiteSpace() {
  // assign blocks to columns
  int sz = (int) BlockList()->size();
  std::vector<int> block_column_assign(sz, -1);
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list_[i].block_count_ = 0;
    col_list_[i].block_list_.clear();
  }

  std::vector<Block> &block_list = *BlockList();
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    int col_num = LocToCol((int) std::round(block_list[i].X()));

    std::vector<int> pos_col;
    std::vector<double> distance;
    if (col_num > 0) {
      pos_col.push_back(col_num - 1);
      distance.push_back(0);
    }
    pos_col.push_back(col_num);
    distance.push_back(0);
    if (col_num < tot_col_num_ - 1) {
      pos_col.push_back(col_num + 1);
      distance.push_back(0);
    }

    Strip *strip = nullptr;
    double min_dist = DBL_MAX;
    for (auto &num: pos_col) {
      double tmp_dist;
      auto res = col_list_[num].GetStripClosestToBlk(&block_list[i], tmp_dist);
      if (tmp_dist < min_dist) {
        strip = res;
        col_num = num;
        min_dist = tmp_dist;
      }
    }
    if (strip != nullptr) {
      col_list_[col_num].block_count_++;
      block_column_assign[i] = col_num;
    }
  }
  for (int i = 0; i < tot_col_num_; ++i) {
    int capacity = col_list_[i].block_count_;
    col_list_[i].block_list_.reserve(capacity);
  }

  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    int col_num = block_column_assign[i];
    if (col_num >= 0) {
      col_list_[col_num].block_list_.push_back(&block_list[i]);
    }
  }

  for (auto &col:col_list_) {
    col.AssignBlockToSimpleStrip();
  }
}

void StdClusterWellLegalizer::AppendBlockToColBottomUp(Strip &strip, Block &blk) {
  bool is_no_cluster_in_col = (strip.contour_ == strip.LLY());
  bool is_new_cluster_needed = is_no_cluster_in_col;
  if (!is_new_cluster_needed) {
    bool is_not_in_top_cluster = strip.contour_ <= blk.LLY();
    bool is_top_cluster_full = strip.front_cluster_->UsedSize() + blk.Width() > strip.width_;
    is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.LLY());
  init_y = std::max(init_y, strip.contour_);

  Cluster *front_cluster;
  auto *blk_well = blk.Type()->GetWell();
  int p_well_height = blk_well->GetPWellHeight();
  int n_well_height = blk_well->GetNWellHeight();
  if (is_new_cluster_needed) {
    strip.cluster_list_.emplace_back();
    front_cluster = &(strip.cluster_list_.back());
    front_cluster->blk_list_.reserve(strip.max_blk_capacity_per_cluster_);
    front_cluster->blk_list_.push_back(&blk);
    //int num_of_tap_cell = (int) std::ceil(strip.Width() / max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightFromBottom(tap_cell_p_height_, tap_cell_n_height_);
    front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
    front_cluster->SetLLY(init_y);
    front_cluster->SetLLX(strip.LLX());
    front_cluster->SetWidth(strip.Width());

    strip.front_cluster_ = front_cluster;
    strip.cluster_count_ += 1;
    strip.used_height_ += front_cluster->Height();
  } else {
    front_cluster = strip.front_cluster_;
    front_cluster->blk_list_.push_back(&blk);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
      strip.used_height_ += front_cluster->Height() - old_height;
    }
  }
  strip.contour_ = front_cluster->URY();
}

void StdClusterWellLegalizer::AppendBlockToColTopDown(Strip &strip, Block &blk) {
  bool is_no_cluster = strip.cluster_list_.empty();
  bool is_new_cluster_needed = is_no_cluster;
  if (!is_new_cluster_needed) {
    bool is_not_in_top_cluster = strip.contour_ >= blk.URY();
    bool is_top_cluster_full = strip.front_cluster_->UsedSize() + blk.Width() > strip.width_;
    is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.URY());
  init_y = std::min(init_y, strip.contour_);

  Cluster *front_cluster;
  auto *blk_well = blk.Type()->GetWell();
  int p_well_height = blk_well->GetPWellHeight();
  int n_well_height = blk_well->GetNWellHeight();
  if (is_new_cluster_needed) {
    strip.cluster_list_.emplace_back();
    front_cluster = &(strip.cluster_list_.back());
    front_cluster->blk_list_.reserve(strip.max_blk_capacity_per_cluster_);
    front_cluster->blk_list_.push_back(&blk);
    //int num_of_tap_cell = (int) std::ceil(strip.Width() / max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightFromTop(tap_cell_p_height_, tap_cell_n_height_);
    front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
    front_cluster->SetURY(init_y);
    front_cluster->SetLLX(strip.LLX());
    front_cluster->SetWidth(strip.Width());

    strip.front_cluster_ = front_cluster;
    strip.cluster_count_ += 1;
    strip.used_height_ += front_cluster->Height();
  } else {
    front_cluster = strip.front_cluster_;
    front_cluster->blk_list_.push_back(&blk);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
      strip.used_height_ += front_cluster->Height() - old_height;
    }
  }
  strip.contour_ = front_cluster->LLY();
}

void StdClusterWellLegalizer::AppendBlockToColBottomUpCompact(Strip &strip, Block &blk) {
  bool is_new_cluster_needed = (strip.contour_ == strip.LLY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full = strip.front_cluster_->UsedSize() + blk.Width() > strip.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.LLY());
  init_y = std::max(init_y, strip.contour_);

  Cluster *front_cluster;
  auto *well = blk.Type()->GetWell();
  int p_well_height = well->GetPWellHeight();
  int n_well_height = well->GetNWellHeight();
  if (is_new_cluster_needed) {
    strip.cluster_list_.emplace_back();
    front_cluster = &(strip.cluster_list_.back());
    front_cluster->blk_list_.reserve(strip.max_blk_capacity_per_cluster_);
    front_cluster->blk_list_.push_back(&blk);
    //int num_of_tap_cell = (int) std::ceil(strip.Width() / max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightFromBottom(tap_cell_p_height_, tap_cell_n_height_);
    front_cluster->SetLLY(init_y);
    front_cluster->SetLLX(strip.LLX());
    front_cluster->SetWidth(strip.Width());
    front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);

    strip.front_cluster_ = front_cluster;
    strip.cluster_count_ += 1;
    strip.used_height_ += front_cluster->Height();
  } else {
    front_cluster = strip.front_cluster_;
    front_cluster->blk_list_.push_back(&blk);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
      strip.used_height_ += front_cluster->Height() - old_height;
    }
  }
  strip.contour_ = front_cluster->URY();
}

void StdClusterWellLegalizer::AppendBlockToColTopDownCompact(Strip &strip, Block &blk) {
  bool is_new_cluster_needed = (strip.contour_ == strip.URY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full = strip.front_cluster_->UsedSize() + blk.Width() > strip.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int) std::round(blk.URY());
  init_y = std::min(init_y, strip.contour_);

  Cluster *front_cluster;
  auto *well = blk.Type()->GetWell();
  int p_well_height = well->GetPWellHeight();
  int n_well_height = well->GetNWellHeight();
  if (is_new_cluster_needed) {
    strip.cluster_list_.emplace_back();
    front_cluster = &(strip.cluster_list_.back());
    front_cluster->blk_list_.reserve(strip.max_blk_capacity_per_cluster_);
    front_cluster->blk_list_.push_back(&blk);
    //int num_of_tap_cell = (int) std::ceil(strip.Width() / max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightFromTop(tap_cell_p_height_, tap_cell_n_height_);
    front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
    front_cluster->SetURY(init_y);
    front_cluster->SetLLX(strip.LLX());
    front_cluster->SetWidth(strip.Width());

    strip.front_cluster_ = front_cluster;
    strip.cluster_count_ += 1;
    strip.used_height_ += front_cluster->Height();
  } else {
    front_cluster = strip.front_cluster_;
    front_cluster->blk_list_.push_back(&blk);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
      strip.used_height_ += front_cluster->Height() - old_height;
    }
  }
  strip.contour_ = front_cluster->LLY();
}

bool StdClusterWellLegalizer::StripLegalizationBottomUp(Strip &strip) {
  strip.cluster_list_.clear();
  strip.contour_ = strip.LLY();
  strip.used_height_ = 0;
  strip.cluster_count_ = 0;
  strip.front_cluster_ = nullptr;
  strip.is_bottom_up_ = true;

  std::sort(strip.block_list_.begin(),
            strip.block_list_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr: strip.block_list_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColBottomUp(strip, *blk_ptr);
  }

  for (auto &cluster: strip.cluster_list_) {
    cluster.UpdateBlockLocY();
  }

  return strip.contour_ <= strip.URY();
}

bool StdClusterWellLegalizer::StripLegalizationTopDown(Strip &strip) {
  strip.cluster_list_.clear();
  strip.contour_ = strip.URY();
  strip.used_height_ = 0;
  strip.cluster_count_ = 0;
  strip.front_cluster_ = nullptr;
  strip.is_bottom_up_ = false;

  std::sort(strip.block_list_.begin(),
            strip.block_list_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->URY() > rhs->URY()) || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr: strip.block_list_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColTopDown(strip, *blk_ptr);
  }

  for (auto &cluster: strip.cluster_list_) {
    cluster.UpdateBlockLocY();
  }

  /*std::cout << "Reverse clustering: ";
  if (strip.contour_ >= RegionLLY()) {
    std::cout << "success\n";
  } else {
    std::cout << "fail\n";
  }*/

  return strip.contour_ >= strip.LLY();
}

bool StdClusterWellLegalizer::StripLegalizationBottomUpCompact(Strip &strip) {
  strip.cluster_list_.clear();
  strip.contour_ = RegionBottom();
  strip.used_height_ = 0;
  strip.cluster_count_ = 0;
  strip.front_cluster_ = nullptr;
  strip.is_bottom_up_ = true;

  std::sort(strip.block_list_.begin(),
            strip.block_list_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr: strip.block_list_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColBottomUpCompact(strip, *blk_ptr);
  }

  for (auto &cluster: strip.cluster_list_) {
    cluster.UpdateBlockLocY();
  }

  return strip.contour_ <= RegionTop();
}

bool StdClusterWellLegalizer::StripLegalizationTopDownCompact(Strip &strip) {
  strip.cluster_list_.clear();
  strip.contour_ = strip.URY();
  strip.used_height_ = 0;
  strip.cluster_count_ = 0;
  strip.front_cluster_ = nullptr;
  strip.is_bottom_up_ = false;

  std::sort(strip.block_list_.begin(),
            strip.block_list_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->URY() > rhs->URY()) || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr: strip.block_list_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColTopDownCompact(strip, *blk_ptr);
  }

  for (auto &cluster: strip.cluster_list_) {
    cluster.UpdateBlockLocY();
  }

  /*std::cout << "Reverse clustering: ";
  if (strip.contour_ >= RegionLLY()) {
    std::cout << "success\n";
  } else {
    std::cout << "fail\n";
  }*/

  return strip.contour_ >= RegionBottom();
}

bool StdClusterWellLegalizer::BlockClustering() {
  /****
   * Clustering blocks in each strip
   * After clustering, close pack clusters from bottom to top
   ****/
  bool res = true;
  for (auto &col: col_list_) {
    bool is_success = true;
    for (auto &strip: col.strip_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripLegalizationBottomUp(strip);
        if (!is_success) {
          is_success = StripLegalizationTopDown(strip);
        }
      }
      res = res && is_success;

      // close pack clusters from bottom to top
      strip.contour_ = strip.LLY();
      if (strip.is_bottom_up_) {
        for (auto &cluster: strip.cluster_list_) {
          strip.contour_ += cluster.Height();
          cluster.SetLLY(strip.contour_);
          cluster.UpdateBlockLocY();
          cluster.LegalizeCompactX();
        }
      } else {
        int sz = strip.cluster_list_.size();
        for (int i = sz - 1; i >= 0; --i) {
          auto &cluster = strip.cluster_list_[i];
          strip.contour_ += cluster.Height();
          cluster.SetLLY(strip.contour_);
          cluster.UpdateBlockLocY();
          cluster.LegalizeCompactX();
        }
      }
    }
  }
  return res;
}

bool StdClusterWellLegalizer::BlockClusteringLoose() {
  /****
   * Clustering blocks in each strip
   * After clustering, leave clusters as they are
   * ****/

  bool res = true;
  for (auto &col: col_list_) {
    bool is_success = true;
    for (auto &strip: col.strip_list_) {
      int i = 0;
      bool is_from_bottom = true;
      for (i = 0; i < max_iter_; ++i) {
        if (is_from_bottom) {
          is_success = StripLegalizationBottomUp(strip);
        } else {
          is_success = StripLegalizationTopDown(strip);
        }
        if (!is_success) {
          is_success = TrialClusterLegalization(strip);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
      /*if (is_success) {
        printf("strip legalization success, %d\n", i);
      } else {
        printf("strip legalization fail, %d\n", i);
      }*/

      for (auto &cluster: strip.cluster_list_) {
        cluster.UpdateBlockLocY();
        cluster.LegalizeLooseX();
      }
    }
  }

  return res;
}

bool StdClusterWellLegalizer::BlockClusteringCompact() {
  /****
   * Clustering blocks in each strip in a compact way
   * After clustering, leave clusters as they are
   * ****/

  bool res = true;
  for (auto &col: col_list_) {
    bool is_success = true;
    for (auto &strip: col.strip_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripLegalizationBottomUpCompact(strip);
        if (!is_success) {
          is_success = StripLegalizationTopDownCompact(strip);
        }
      }
      res = res && is_success;

      for (auto &cluster: strip.cluster_list_) {
        cluster.UpdateBlockLocY();
        cluster.LegalizeLooseX();
      }
    }
  }

  return res;
}

bool StdClusterWellLegalizer::TrialClusterLegalization(Strip &strip) {
  /****
   * Legalize the location of all clusters using extended Tetris legalization algorithm in columns where usage does not exceed capacity
   * Closely pack the column from bottom to top if its usage exceeds its capacity
   * ****/

  bool res = true;

  // sort clusters in each column based on the lower left corner
  std::vector<Cluster *> cluster_list;
  cluster_list.resize(strip.cluster_count_, nullptr);
  for (int i = 0; i < strip.cluster_count_; ++i) {
    cluster_list[i] = &strip.cluster_list_[i];
  }

  //std::cout << "used height/RegionHeight(): " << col.used_height_ / (double) RegionHeight() << "\n";
  if (strip.used_height_ <= RegionHeight()) {
    if (strip.is_bottom_up_) {
      std::sort(cluster_list.begin(),
                cluster_list.end(),
                [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->URY() > rhs->URY());
                });
      int cluster_contour = strip.URY();
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
      std::sort(cluster_list.begin(),
                cluster_list.end(),
                [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->LLY() < rhs->LLY());
                });
      int cluster_contour = strip.LLY();
      int res_y;
      int init_y;
      for (auto &cluster: cluster_list) {
        init_y = cluster->LLY();
        res_y = std::max(cluster_contour, cluster->LLY());
        cluster->SetLLY(res_y);
        cluster_contour = cluster->URY();
        cluster->ShiftBlockY(res_y - init_y);
      }
    }
  } else {
    std::sort(cluster_list.begin(),
              cluster_list.end(),
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

  return res;
}

double StdClusterWellLegalizer::WireLengthCost(Cluster *cluster, int l, int r) {
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
    for (auto &net_num: *blk->NetList()) {
      if (net_list[net_num].P() < 100) {
        net_involved.insert(&(net_list[net_num]));
      }
    }
  }

  double hpwl_x = 0;
  double hpwl_y = 0;
  for (auto &net: net_involved) {
    hpwl_x += net->HPWLX();
    hpwl_y += net->HPWLY();
  }

  return hpwl_x * circuit_->GetGridValueX() + hpwl_y * circuit_->GetGridValueY();
}

void StdClusterWellLegalizer::FindBestLocalOrder(std::vector<Block *> &res,
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
    cluster->blk_list_[l]->setLLX(left_bound);
    cluster->blk_list_[r]->setURX(right_bound);

    int left_contour = left_bound + gap + cluster->blk_list_[l]->Width();
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->blk_list_[i];
      blk->setLLX(left_contour);
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

void StdClusterWellLegalizer::LocalReorderInCluster(Cluster *cluster, int range) {
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

    cluster->blk_list_[l]->setLLX(left_bound);
    cluster->blk_list_[r]->setURX(right_bound);
    int left_contour = left_bound + cluster->blk_list_[l]->Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->blk_list_[i];
      blk->setLLX(left_contour);
      left_contour += blk->Width() + gap;
    }
  }

}

void StdClusterWellLegalizer::LocalReorderAllClusters() {
  // sort cluster based on the lower left corner
  int tot_cluster_count = 0;
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      tot_cluster_count += strip.cluster_list_.size();
    }
  }
  std::vector<Cluster *> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      for (auto &cluster: strip.cluster_list_) {
        cluster_ptr_list[counter] = &cluster;
        ++counter;
      }
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

void StdClusterWellLegalizer::SingleSegmentClusteringOptimization() {

}

void StdClusterWellLegalizer::UpdateClusterOrient() {
  for (auto &col: col_list_) {
    bool is_orient_N = is_first_row_orient_N_;
    for (auto &strip: col.strip_list_) {
      if (strip.is_bottom_up_) {
        for (auto &cluster: strip.cluster_list_) {
          cluster.SetOrient(is_orient_N);
          is_orient_N = !is_orient_N;
        }
      } else {
        int sz = strip.cluster_list_.size();
        for (int i = sz - 1; i >= 0; --i) {
          strip.cluster_list_[i].SetOrient(is_orient_N);
          is_orient_N = !is_orient_N;
        }
      }
    }
  }
}

void StdClusterWellLegalizer::InsertWellTap() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Inserting well tap cell\n";
  }

  auto &tap_cell_list = circuit_->design_.well_tap_list;
  tap_cell_list.clear();
  int tot_cluster_count = 0;
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      tot_cluster_count += strip.cluster_list_.size();
    }
  }
  tap_cell_list.reserve(tot_cluster_count * 2);
  circuit_->design_.tap_name_map.clear();

  int counter = 0;
  int tot_tap_cell_num = 0;
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      for (auto &cluster: strip.cluster_list_) {
        int tap_cell_num = std::ceil(cluster.Width() / (double) max_unplug_length_);
        tot_tap_cell_num += tap_cell_num;
        int step = 2 * max_unplug_length_;
        int tap_cell_loc = cluster.LLX() - well_tap_cell_->Width() / 2;
        for (int i = 0; i < tap_cell_num; ++i) {
          std::string block_name = "__well_tap__" + std::to_string(counter++);
          tap_cell_list.emplace_back();
          auto &tap_cell = tap_cell_list.back();
          tap_cell.setPlacementStatus(PLACED_);
          tap_cell.setType(circuit_->tech_.WellTapCell());
          int map_size = circuit_->design_.tap_name_map.size();
          auto ret = circuit_->design_.tap_name_map.insert(std::pair<std::string, int>(block_name, map_size));
          auto *name_num_pair_ptr = &(*ret.first);
          tap_cell.setNameNumPair(name_num_pair_ptr);
          cluster.InsertWellTapCell(tap_cell, tap_cell_loc);
          tap_cell_loc += step;
        }
        cluster.LegalizeLooseX(space_to_well_tap_);
      }
    }
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Inserting complete: %d well tap cell created\n", tot_tap_cell_num);
  }
}

void StdClusterWellLegalizer::ClearCachedData() {
  for (auto &block: *BlockList()) {
    block.setOrient(N_);
  }

  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      strip.contour_ = strip.LLY();
      strip.used_height_ = 0;
      strip.cluster_count_ = 0;
      strip.cluster_list_.clear();
      strip.front_cluster_ = nullptr;
    }
  }

  //cluster_list_.clear();
}

bool StdClusterWellLegalizer::WellLegalize() {
  ClearCachedData();
  bool is_success;

  AssignBlockToColBasedOnWhiteSpace();
  is_success = BlockClusteringLoose();
  ReportHPWL(LOG_CRITICAL);

  UpdateClusterOrient();
  for (int i = 0; i < 6; ++i) {
    LocalReorderAllClusters();
    ReportHPWL(LOG_CRITICAL);
  }

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

bool StdClusterWellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Standard Cluster Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  /****---->****/
  bottom_ += 1;
  top_ -= 1;

  //circuit_->GenMATLABWellTable("lg", false);

  bool is_success = true;
  Init();
  AssignBlockToColBasedOnWhiteSpace();
  //BlockClustering();
  is_success = BlockClusteringLoose();
  //BlockClusteringCompact();
  ReportHPWL(LOG_CRITICAL);
  //circuit_->GenMATLABWellTable("clu", false);
  //GenMatlabClusterTable("clu_result");

  UpdateClusterOrient();
  //circuit_->GenMATLABWellTable("ori", false);
  //GenMatlabClusterTable("ori_result");
  ReportHPWL(LOG_CRITICAL);
  for (int i = 0; i < 6; ++i) {
    LocalReorderAllClusters();
    ReportHPWL(LOG_CRITICAL);
  }
  //circuit_->GenMATLABWellTable("lop", false);
  //GenMatlabClusterTable("lop_result");

  InsertWellTap();
  //circuit_->GenMATLABWellTable("wtc", false);
  //GenMatlabClusterTable("wtc_result");
  ReportHPWL(LOG_CRITICAL);

  bottom_ -= 1;
  top_ += 1;

  if (globalVerboseLevel >= LOG_CRITICAL) {
    if (is_success) {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization complete!\n"
                << "\033[0m";
    } else {
      std::cout << "\033[0;36m"
                << "Standard Cluster Well Legalization fail!\n"
                << "Please try lower density"
                << "\033[0m";
    }
  }
  /****<----****/

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("(wall time: %.4fs, cpu time: %.4fs)\n", wall_time, cpu_time);
  }

  ReportMemory(LOG_CRITICAL);

  //ReportEffectiveSpaceUtilization();

  return is_success;
}

void StdClusterWellLegalizer::ReportEffectiveSpaceUtilization() {
  long int tot_std_blk_area = 0;
  int max_n_height = 0;
  int max_p_height = 0;
  for (auto &blk: circuit_->design_.block_list) {
    BlockType *type = blk.Type();
    if (type == circuit_->tech_.io_dummy_blk_type_ptr_) continue;;
    if (type->GetWell()->GetNWellHeight() > max_n_height) {
      max_n_height = type->GetWell()->GetNWellHeight();
    }
    if (type->GetWell()->GetPWellHeight() > max_p_height) {
      max_p_height = type->GetWell()->GetPWellHeight();
    }
  }
  if (circuit_->tech_.well_tap_cell_ptr_->GetWell()->GetNWellHeight() > max_n_height) {
    max_n_height = circuit_->tech_.well_tap_cell_ptr_->GetWell()->GetNWellHeight();
  }
  if (circuit_->tech_.well_tap_cell_ptr_->GetWell()->GetPWellHeight() > max_p_height) {
    max_p_height = circuit_->tech_.well_tap_cell_ptr_->GetWell()->GetPWellHeight();
  }
  int max_height = max_n_height + max_p_height;

  long int tot_eff_blk_area = 0;
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      for (auto &cluster: strip.cluster_list_) {
        long int eff_height = cluster.Height();
        long int tot_cell_width = 0;
        for (auto &blk_ptr: cluster.blk_list_) {
          tot_cell_width += blk_ptr->Width();
        }
        tot_eff_blk_area += tot_cell_width * eff_height;
        tot_std_blk_area += tot_cell_width * max_height;
      }
    }
  }
  double factor = circuit_->tech_.grid_value_x_ * circuit_->tech_.grid_value_y_;
  std::cout << "Total placement area: " << ((long int) RegionWidth() * (long int) RegionHeight()) * factor << " um^2\n";
  std::cout << "Total block area: " << circuit_->TotBlkArea() * factor << " ("
            << circuit_->TotBlkArea() / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";
  std::cout << "Total effective block area: " << tot_eff_blk_area * factor << " ("
            << tot_eff_blk_area / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";
  std::cout << "Total standard block area (lower bound):" << tot_std_blk_area * factor << " ("
            << tot_std_blk_area / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";

}

void StdClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);

  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &col:col_list_) {
    for (auto &strip: col.strip_list_) {
      for (auto &cluster: strip.cluster_list_) {
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
  }
  ost.close();
}

void StdClusterWellLegalizer::GenMATLABWellTable(std::string const &name_of_file, int well_emit_mode) {
  circuit_->GenMATLABWellTable(name_of_file, false);

  std::string p_file = name_of_file + "_pwell.txt";
  std::ofstream ostp(p_file.c_str());
  Assert(ostp.is_open(), "Cannot open output file: " + p_file);

  std::string n_file = name_of_file + "_nwell.txt";
  std::ofstream ostn(n_file.c_str());
  Assert(ostn.is_open(), "Cannot open output file: " + n_file);

  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      std::vector<int> pn_edge_list;
      if (strip.is_bottom_up_) {
        pn_edge_list.reserve(strip.cluster_list_.size() + 2);
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.reserve(strip.cluster_list_.size() + 2);
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = strip.is_first_row_orient_N_;
      int lx = strip.LLX();
      int ux = strip.URX();
      int ly;
      int uy;
      int rect_count = (int) pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          if (well_emit_mode != 1) {
            ostp << lx << "\t"
                 << ux << "\t"
                 << ux << "\t"
                 << lx << "\t"
                 << ly << "\t"
                 << ly << "\t"
                 << uy << "\t"
                 << uy << "\n";
          }
        } else {
          if (well_emit_mode != 2) {
            ostn << lx << "\t"
                 << ux << "\t"
                 << ux << "\t"
                 << lx << "\t"
                 << ly << "\t"
                 << ly << "\t"
                 << uy << "\t"
                 << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ostp.close();
  ostn.close();

  GenPPNP(name_of_file);
}

void StdClusterWellLegalizer::GenPPNP(const std::string &name_of_file) {
  std::string np_file = name_of_file + "_np.txt";
  std::ofstream ostnp(np_file.c_str());
  Assert(ostnp.is_open(), "Cannot open output file: " + np_file);

  std::string pp_file = name_of_file + "_pp.txt";
  std::ofstream ostpp(pp_file.c_str());
  Assert(ostpp.is_open(), "Cannot open output file: " + pp_file);

  int adjust_width = well_tap_cell_->Width();

  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(strip.cluster_list_.size() + 2);
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = strip.is_first_row_orient_N_;
      int lx = strip.LLX();
      int ux = strip.URX();
      int ly;
      int uy;
      int rect_count = (int) pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          ostnp << lx + adjust_width << "\t"
                << ux - adjust_width << "\t"
                << ux - adjust_width << "\t"
                << lx + adjust_width << "\t"
                << ly << "\t"
                << ly << "\t"
                << uy << "\t"
                << uy << "\n";
        } else {
          ostpp << lx + adjust_width << "\t"
                << ux - adjust_width << "\t"
                << ux - adjust_width << "\t"
                << lx + adjust_width << "\t"
                << ly << "\t"
                << ly << "\t"
                << uy << "\t"
                << uy << "\n";
        }
        is_p_well_rect = !is_p_well_rect;
      }

      // draw NP and PP shapes from well-tap cell to well-tap cell
      std::vector<int> well_tap_top_bottom_list;
      well_tap_top_bottom_list.reserve(strip.cluster_list_.size() + 2);
      if (strip.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionBottom());
      } else {
        well_tap_top_bottom_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        if (strip.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
        }
      }
      if (strip.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionTop());
      } else {
        well_tap_top_bottom_list.push_back(RegionBottom());
        std::reverse(well_tap_top_bottom_list.begin(), well_tap_top_bottom_list.end());
      }
      Assert(well_tap_top_bottom_list.size() % 2 == 0, "Impossible to get an even number of well tap cell edges");

      is_p_well_rect = strip.is_first_row_orient_N_;
      int lx0 = strip.LLX();
      int ux0 = lx + adjust_width;
      int ux1 = strip.URX();
      int lx1 = ux1 - adjust_width;
      rect_count = (int) well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          if (is_p_well_rect) {
            ostpp << lx0 << "\t"
                  << ux0 << "\t"
                  << ux0 << "\t"
                  << lx0 << "\t"
                  << ly << "\t"
                  << ly << "\t"
                  << uy << "\t"
                  << uy << "\n";
            ostpp << lx1 << "\t"
                  << ux1 << "\t"
                  << ux1 << "\t"
                  << lx1 << "\t"
                  << ly << "\t"
                  << ly << "\t"
                  << uy << "\t"
                  << uy << "\n";
          } else {
            ostnp << lx0 << "\t"
                  << ux0 << "\t"
                  << ux0 << "\t"
                  << lx0 << "\t"
                  << ly << "\t"
                  << ly << "\t"
                  << uy << "\t"
                  << uy << "\n";
            ostnp << lx1 << "\t"
                  << ux1 << "\t"
                  << ux1 << "\t"
                  << lx1 << "\t"
                  << ly << "\t"
                  << ly << "\t"
                  << uy << "\t"
                  << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ostnp.close();
  ostpp.close();
}

void StdClusterWellLegalizer::EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode) {
  /****
   * Emit three files:
   * 1. rect file including all N/P well rectangles
   * 2. rect file including all NP/PP rectangles
   * 3. cluster file including all cluster shapes
   *
   * @param well_mode
   * 0: emit both N-well and P-well
   * 1: emit N-well only
   * 2: emit P-well only
   * ****/

  EmitPPNPRect(name_of_file + "ppnp.rect");
  EmitWellRect(name_of_file + "well.rect", well_emit_mode);
  EmitClusterRect(name_of_file + "_router.cluster");
}

void StdClusterWellLegalizer::EmitPPNPRect(std::string const &name_of_file) {
  // emit rect file
  std::string NP_name = "nplus";
  std::string PP_name = "pplus";

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Writing PP and NP rect file '%s', ", name_of_file.c_str());
  }

  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_x_;
  double factor_y = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_y_;

  ost << "bbox "
      << (int) (RegionLeft() * factor_x) + circuit_->design_.die_area_offset_x_ << " "
      << (int) (RegionBottom() * factor_y) + circuit_->design_.die_area_offset_y_ << " "
      << (int) (RegionRight() * factor_x) + circuit_->design_.die_area_offset_x_ << " "
      << (int) (RegionTop() * factor_y) + circuit_->design_.die_area_offset_y_ << "\n";

  int adjust_width = well_tap_cell_->Width();

  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(strip.cluster_list_.size() + 2);
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = strip.is_first_row_orient_N_;
      int lx = strip.LLX();
      int ux = strip.URX();
      int ly;
      int uy;
      int rect_count = (int) pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          ost << "rect # " << NP_name << " ";
        } else {
          ost << "rect # " << PP_name << " ";
        }
        ost << (lx + adjust_width) * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
            << ly * factor_y + circuit_->design_.die_area_offset_y_ << "\t"
            << (ux - adjust_width) * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
            << uy * factor_y + circuit_->design_.die_area_offset_y_ << "\n";

        is_p_well_rect = !is_p_well_rect;
      }

      // draw NP and PP shapes from well-tap cell to well-tap cell
      std::vector<int> well_tap_top_bottom_list;
      well_tap_top_bottom_list.reserve(strip.cluster_list_.size() + 2);
      if (strip.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionBottom());
      } else {
        well_tap_top_bottom_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        if (strip.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
        }
      }
      if (strip.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionTop());
      } else {
        well_tap_top_bottom_list.push_back(RegionBottom());
        std::reverse(well_tap_top_bottom_list.begin(), well_tap_top_bottom_list.end());
      }
      Assert(well_tap_top_bottom_list.size() % 2 == 0, "Impossible to get an even number of well tap cell edges");

      is_p_well_rect = strip.is_first_row_orient_N_;
      int lx0 = strip.LLX();
      int ux0 = lx + adjust_width;
      int ux1 = strip.URX();
      int lx1 = ux1 - adjust_width;
      rect_count = (int) well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          if (!is_p_well_rect) {
            ost << "rect # " << NP_name << " ";
          } else {
            ost << "rect # " << PP_name << " ";
          }
          ost << lx0 * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
              << ly * factor_y + circuit_->design_.die_area_offset_y_ << "\t"
              << ux0 * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
              << uy * factor_y + circuit_->design_.die_area_offset_y_ << "\n";
          if (!is_p_well_rect) {
            ost << "rect # " << NP_name << " ";
          } else {
            ost << "rect # " << PP_name << " ";
          }
          ost << lx1 * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
              << ly * factor_y + circuit_->design_.die_area_offset_y_ << "\t"
              << ux1 * factor_x + circuit_->design_.die_area_offset_x_ << "\t"
              << uy * factor_y + circuit_->design_.die_area_offset_y_ << "\n";
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ost.close();
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void StdClusterWellLegalizer::EmitWellRect(std::string const &name_of_file, int well_emit_mode) {
  // emit rect file
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Writing N/P-well rect file '%s', ", name_of_file.c_str());
  }

  switch (well_emit_mode) {
    case 0:printf("emit N/P wells, ");
      break;
    case 1:printf("emit N wells, ");
      break;
    case 2:printf("emit P wells, ");
      break;
    default:Assert(false, "Invalid value for well_emit_mode in StdClusterWellLegalizer::EmitDEFWellFile()");
  }

  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_x_;
  double factor_y = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_y_;

  ost << "bbox "
      << (int) (RegionLeft() * factor_x) + circuit_->design_.die_area_offset_x_ << " "
      << (int) (RegionBottom() * factor_y) + circuit_->design_.die_area_offset_y_ << " "
      << (int) (RegionRight() * factor_x) + circuit_->design_.die_area_offset_x_ << " "
      << (int) (RegionTop() * factor_y) + circuit_->design_.die_area_offset_y_ << "\n";
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      std::vector<int> pn_edge_list;
      if (strip.is_bottom_up_) {
        pn_edge_list.reserve(strip.cluster_list_.size() + 2);
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.reserve(strip.cluster_list_.size() + 2);
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster: strip.cluster_list_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (strip.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = strip.is_first_row_orient_N_;
      int lx = strip.LLX();
      int ux = strip.URX();
      int ly;
      int uy;
      int rect_count = (int) pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          is_p_well_rect = !is_p_well_rect;
          if (well_emit_mode == 1) continue;
          ost << "rect GND pwell ";
        } else {
          is_p_well_rect = !is_p_well_rect;
          if (well_emit_mode == 2) continue;
          ost << "rect Vdd nwell ";
        }
        ost << (int) (lx * factor_x) + circuit_->design_.die_area_offset_x_ << " "
            << (int) (ly * factor_y) + circuit_->design_.die_area_offset_y_ << " "
            << (int) (ux * factor_x) + circuit_->design_.die_area_offset_x_ << " "
            << (int) (uy * factor_y) + circuit_->design_.die_area_offset_y_ << "\n";
      }
    }
  }
  ost.close();
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void StdClusterWellLegalizer::EmitClusterRect(std::string const &name_of_file) {
  /****
   * Emits a rect file for power routing
   * ****/

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Writing cluster rect file '%s' for router, ", name_of_file.c_str());
  }
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_x_;
  double factor_y = circuit_->design_.def_distance_microns * circuit_->tech_.grid_value_y_;
  //std::cout << "Actual x span: "
  //          << RegionLeft() * factor_x + +circuit_->design_.die_area_offset_x_ << "  "
  //          << (col_list_.back().strip_list_[0].URX() + well_spacing_) * factor_x + circuit_->design_.die_area_offset_x_
  //          << "\n";
  for (int i = 0; i < tot_col_num_; ++i) {
    std::string column_name = "column" + std::to_string(i);
    ost << "STRIP " << column_name << "\n";

    auto &col = col_list_[i];
    for (auto &strip: col.strip_list_) {
      ost << "  "
          << (int) (strip.LLX() * factor_x) + circuit_->design_.die_area_offset_x_ << "  "
          << (int) (strip.URX() * factor_x) + circuit_->design_.die_area_offset_x_ << "  ";
      if (strip.is_first_row_orient_N_) {
        ost << "GND\n";
      } else {
        ost << "Vdd\n";
      }

      if (strip.is_bottom_up_) {
        for (auto &cluster: strip.cluster_list_) {
          ost << "  "
              << (int) (cluster.LLY() * factor_y) + circuit_->design_.die_area_offset_y_ << "  "
              << (int) (cluster.URY() * factor_y) + circuit_->design_.die_area_offset_y_ << "\n";
        }
      } else {
        int sz = strip.cluster_list_.size();
        for (int j = sz - 1; j >= 0; --j) {
          auto &cluster = strip.cluster_list_[j];
          ost << "  "
              << (int) (cluster.LLY() * factor_y) + circuit_->design_.die_area_offset_y_ << "  "
              << (int) (cluster.URY() * factor_y) + circuit_->design_.die_area_offset_y_ << "\n";
        }
      }

      ost << "END " << column_name << "\n\n";
    }
  }
  ost.close();
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void StdClusterWellLegalizer::GenAvailSpace(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << RegionLeft() << "\t"
      << RegionRight() << "\t"
      << RegionRight() << "\t"
      << RegionLeft() << "\t"
      << RegionBottom() << "\t"
      << RegionBottom() << "\t"
      << RegionTop() << "\t"
      << RegionTop() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto &row = white_space_in_rows_[i];
    for (auto &seg: row) {
      ost << seg.lo << "\t"
          << seg.hi << "\t"
          << seg.hi << "\t"
          << seg.lo << "\t"
          << i * row_height_ + RegionBottom() << "\t"
          << i * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\t"
          << 1 << "\t"
          << 1 << "\t"
          << 1 << "\n";
    }
  }

  for (auto &block: *BlockList()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 1 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}

void StdClusterWellLegalizer::GenAvailSpaceInCols(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << RegionLeft() << "\t"
      << RegionRight() << "\t"
      << RegionRight() << "\t"
      << RegionLeft() << "\t"
      << RegionBottom() << "\t"
      << RegionBottom() << "\t"
      << RegionTop() << "\t"
      << RegionTop() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (auto &col: col_list_) {
    for (int i = 0; i < tot_num_rows_; ++i) {
      auto &row = col.white_space_[i];
      for (auto &seg: row) {
        ost << seg.lo << "\t"
            << seg.hi << "\t"
            << seg.hi << "\t"
            << seg.lo << "\t"
            << i * row_height_ + RegionBottom() << "\t"
            << i * row_height_ + RegionBottom() << "\t"
            << (i + 1) * row_height_ + RegionBottom() << "\t"
            << (i + 1) * row_height_ + RegionBottom() << "\t"
            << 0 << "\t"
            << 1 << "\t"
            << 1 << "\n";
      }
    }
  }

  for (auto &block: *BlockList()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 0 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}

void StdClusterWellLegalizer::GenSimpleStrips(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << RegionLeft() << "\t"
      << RegionRight() << "\t"
      << RegionRight() << "\t"
      << RegionLeft() << "\t"
      << RegionBottom() << "\t"
      << RegionBottom() << "\t"
      << RegionTop() << "\t"
      << RegionTop() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (auto &col: col_list_) {
    for (auto &strip: col.strip_list_) {
      ost << strip.LLX() << "\t"
          << strip.URX() << "\t"
          << strip.URX() << "\t"
          << strip.LLX() << "\t"
          << strip.LLY() << "\t"
          << strip.LLY() << "\t"
          << strip.URY() << "\t"
          << strip.URY() << "\t"
          << 0.8 << "\t"
          << 0.8 << "\t"
          << 0.8 << "\n";
    }
  }

  for (auto &block: *BlockList()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 0 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}
