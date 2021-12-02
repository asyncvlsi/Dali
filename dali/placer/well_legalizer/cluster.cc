/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "cluster.h"

#include <algorithm>

namespace dali {

bool Cluster::IsOrientN() const {
  return is_orient_N_;
}

bool Cluster::IsSingle() const {
  return is_only_single_well_cells_;
}

void Cluster::SetIsSingle(bool is_single) {
  is_only_single_well_cells_ = is_single;
}

int Cluster::UsedSize() const {
  return used_size_;
}

void Cluster::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

void Cluster::UseSpace(int width) {
  used_size_ += width;
}

void Cluster::SetUsedSize1(int used_size) {
  used_size1_ = used_size;
}

void Cluster::SetLLX(int lx) {
  lx_ = lx;
}

void Cluster::SetURX(int ux) {
  lx_ = ux - width_;
}

int Cluster::LLX() const {
  return lx_;
}

int Cluster::URX() const {
  return lx_ + width_;
}

double Cluster::CenterX() const {
  return lx_ + width_ / 2.0;
}

void Cluster::SetWidth(int width) {
  width_ = width;
}

int Cluster::Width() const {
  return width_;
}

void Cluster::SetLLY(int ly) {
  ly_ = ly;
}

void Cluster::SetURY(int uy) {
  ly_ = uy - Height();
}

int Cluster::LLY() const {
  return ly_;
}

int Cluster::URY() const {
  return ly_ + Height();
}

double Cluster::CenterY() const {
  return ly_ + height_ / 2.0;
}

void Cluster::SetHeight(int height) {
  height_ = height;
}

/****
* Update the height of this cluster with the lower y of this cluster fixed.
* So even if the height changes, the lower y of this cluster does not need be changed.
* ****/
void Cluster::UpdateWellHeightFromBottom(int p_well_height, int n_well_height) {
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
}

/****
* Update the height of this cluster with the upper y of this cluster fixed.
* So if the height changes, then the lower y of this cluster should also be changed.
* ****/
void Cluster::UpdateWellHeightFromTop(int p_well_height, int n_well_height) {
  int old_height = height_;
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
  ly_ -= (height_ - old_height);
}

int Cluster::Height() const {
  return height_ + height1_;
}

int Cluster::PHeight() const {
  return p_well_height_;
}

int Cluster::NHeight() const {
  return n_well_height_;
}

/****
* Returns the P/N well edge to the bottom of this cluster
* ****/
int Cluster::PNEdge() const {
  return is_orient_N_ ? PHeight() : NHeight();
}

void Cluster::UpdateWellHeightFromBottom1(
    int p_well_height,
    int n_well_height
) {
  p_well_height1_ = std::max(p_well_height1_, p_well_height);
  n_well_height1_ = std::max(n_well_height1_, n_well_height);
  height1_ = p_well_height1_ + n_well_height1_;
}

void Cluster::UpdateWellHeightFromTop1(int p_well_height, int n_well_height) {
  int old_height = height1_;
  p_well_height1_ = std::max(p_well_height1_, p_well_height);
  n_well_height1_ = std::max(n_well_height1_, n_well_height);
  height1_ = p_well_height1_ + n_well_height1_;
  ly_ -= (height_ - old_height);
}

int Cluster::PHeight1() const {
  return p_well_height1_;
}

int Cluster::NHeight1() const {
  return n_well_height1_;
}

int Cluster::PNEdge1() const {
  return is_orient_N_ ? NHeight1() : PHeight1();
}

int Cluster::ClusterEdge() const {
  return p_well_height_ + n_well_height_;
}

void Cluster::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
}

void Cluster::AddBlock(Block *blk_ptr) {
  BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
  blk_list_.push_back(blk_ptr);
  blk_initial_location_.emplace_back(
      blk_ptr->LLX(),
      blk_ptr->LLY() + well_ptr->Pheight()
  );
}

std::vector<Block *> &Cluster::Blocks() {
  return blk_list_;
}

std::vector<double2d> &Cluster::InitLocations() {
  return blk_initial_location_;
}

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
  for (auto &blk_ptr: blk_list_) {
    auto *well = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->SetLLY(ly_ + p_well_height_ - well->Pheight());
  }
}

void Cluster::LegalizeCompactX(int left) {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->LLX() < blk_ptr1->LLX();
      }
  );
  int current_x = left;
  for (auto &blk: blk_list_) {
    blk->SetLLX(current_x);
    current_x += blk->Width();
  }
}

void Cluster::LegalizeCompactX() {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->LLX() < blk_ptr1->LLX();
      }
  );
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->SetLLX(current_x);
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
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->LLX() < blk_ptr1->LLX();
      }
  );
  int block_contour = lx_;
  int res_x;
  for (auto &blk: blk_list_) {
    res_x = std::max(block_contour, int(blk->LLX()));
    blk->SetLLX(res_x);
    block_contour = int(blk->URX());
    if ((tap_cell_ != nullptr) && (blk->TypePtr() == tap_cell_->TypePtr())) {
      block_contour += space_to_well_tap;
    }
  }

  int ux = lx_ + width_;
  //if (block_contour > ux) {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->URX() > blk_ptr1->URX();
      }
  );
  block_contour = ux;
  for (auto &blk: blk_list_) {
    res_x = std::min(block_contour, int(blk->URX()));
    blk->SetURX(res_x);
    block_contour = int(blk->LLX());
    if ((tap_cell_ != nullptr) && (blk->TypePtr() == tap_cell_->TypePtr())) {
      block_contour -= space_to_well_tap;
    }
  }
  //}
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

void Cluster::InsertWellTapCell(Block &tap_cell, int loc) {
  tap_cell_ = &tap_cell;
  blk_list_.emplace_back(tap_cell_);
  tap_cell_->SetCenterX(loc);
  auto *well = tap_cell.TypePtr()->WellPtr();
  int p_well_height = well->Pheight();
  int n_well_height = well->Nheight();
  if (is_orient_N_) {
    tap_cell.SetOrient(N);
    tap_cell.SetLLY(ly_ + p_well_height_ - p_well_height);
  } else {
    tap_cell.SetOrient(FS);
    tap_cell.SetLLY(ly_ + n_well_height_ - n_well_height);
  }
}

void Cluster::UpdateBlockLocationCompact() {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->LLX() < blk_ptr1->LLX();
      }
  );
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->SetLLX(current_x);
    blk->SetCenterY(CenterY());
    current_x += blk->Width();
  }
}

void Cluster::MinDisplacementLegalization() {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->X() < blk_ptr1->X();
      }
  );

  std::vector<BlockSegment> segments;

  DaliExpects(blk_list_.size() == blk_initial_location_.size(),
              "Block number does not equal initial location number\n");

  size_t sz = blk_list_.size();
  int lower_bound = lx_;
  int upper_bound = lx_ + width_;
  //std::cout << sz << "--";
  for (size_t i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    Block *blk_ptr = blk_list_[i];
    double init_x = blk_initial_location_[i].x;
    if (init_x < lower_bound) {
      init_x = lower_bound;
    }
    if (init_x + blk_ptr->Width() > upper_bound) {
      init_x = upper_bound - blk_ptr->Width();
    }
    segments.emplace_back(blk_ptr, init_x);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    BlockSegment *cur_seg = &(segments[seg_sz - 1]);
    BlockSegment *prev_seg = &(segments[seg_sz - 2]);
    //std::cout << prev_seg->IsNotOnLeft(*cur_seg) << " ";
    while (prev_seg->IsNotOnLeft(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  //int count = 0;
  //std::cout << "...";
  for (auto &seg: segments) {
    seg.UpdateBlockLocation();
    //count += seg.blk_list.size();
    //std::cout << seg.blk_list.size() << " ";
    //seg.Report();
  }
  //std::cout << "--" << count << "\n";
}

void Cluster::UpdateMinDisplacementLLY() {
  DaliExpects(blk_list_.size() == blk_initial_location_.size(),
              "Block count does not equal initial location count\n");
  double sum = 0;
  for (auto &init_loc: blk_initial_location_) {
    double init_np_boundary = init_loc.y;
    sum += init_np_boundary;
  }
  min_displacement_lly_ =
      sum / (int) (blk_initial_location_.size()) - PHeight();
}

double Cluster::MinDisplacementLLY() const {
  return min_displacement_lly_;
}

bool Cluster::IsCloserToLowerCluster(Block &blk) {
  BlockTypeWell *well_ptr = blk.TypePtr()->WellPtr();
  if (!well_ptr->IsSingleWell()) return true;
  double lly = blk.LLY();
  return fabs(lly - ly_) < fabs(lly - (ly_ + ClusterEdge()) - 10);
}

bool Cluster::IsEnoughSpace(Block &blk, bool is_lower_cluster) {
  DaliExpects(!IsSingle(), "Only use this function for double clustering");
  BlockTypeWell *well_ptr = blk.TypePtr()->WellPtr();
  int width = blk.Width();
  bool is_enough_space_lower = used_size_ + width < width_;
  bool is_enough_space_upper = used_size1_ + width < width_;
  if (well_ptr->IsSingleWell()) {
    return is_lower_cluster ? is_enough_space_lower : is_enough_space_upper;
  } else {
    return is_enough_space_lower && is_enough_space_upper;
  }
}

bool Cluster::IsEnoughSpace2(Block &blk) {
  BlockTypeWell *well_ptr = blk.TypePtr()->WellPtr();
  int width = blk.Width();
  if (well_ptr->IsSingleWell()) {
    return used_size_ + width < 2 * width_;
  } else {
    return used_size_ + 2 * width < 2 * width_;
  }
}

void Cluster::AddBlockDoubleCluster(Block &blk, bool is_lower_cluster) {
  BlockTypeWell *well_ptr = blk.TypePtr()->WellPtr();
  bool update_lower_cluster = !well_ptr->IsSingleWell() || is_lower_cluster;
  bool update_upper_cluster = !well_ptr->IsSingleWell() || !is_lower_cluster;
  int width = blk.Width();
  if (!well_ptr->IsSingleWell()) {
    blk_list_double_.push_back(&blk);
  }
  if (update_lower_cluster) {
    if (well_ptr->IsSingleWell()) {
      blk_list_.push_back(&blk);
    }
    used_size_ += width;
    int p_well_height = well_ptr->Pheight();
    int n_well_height = well_ptr->Nheight();
    UpdateWellHeightFromBottom(p_well_height, n_well_height);
  }
  if (update_upper_cluster) {
    if (well_ptr->IsSingleWell()) {
      blk_list1_.push_back(&blk);
    }
    used_size1_ += width;
    int p_well_height = well_ptr->Pheight();
    int n_well_height = well_ptr->Nheight();
    if (!well_ptr->IsSingleWell()) {
      p_well_height = well_ptr->Pheight1();
      n_well_height = well_ptr->Nheight1();
    }
    UpdateWellHeightFromBottom1(p_well_height, n_well_height);
  }
}

void Cluster::AddBlockDoubleCluster2(Block &blk) {
  BlockTypeWell *well_ptr = blk.TypePtr()->WellPtr();
  if (well_ptr->IsSingleWell()) {
    blk_list_.push_back(&blk);
  } else {
    blk_list_double_.push_back(&blk);
  }
}

void Cluster::SplitSingleWellCellList() {
  int total_width = 0;
  for (auto &blk_ptr: blk_list_) {
    total_width += blk_ptr->Width();
  }

  int half_width = total_width / 2;
  size_t cut_index = 0;
  size_t sz = blk_list_.size();
  int accum_width = 0;
  for (size_t i = 0; i < sz; ++i) {
    accum_width += blk_list_[i]->Width();
    if (accum_width > half_width) {
      cut_index = i;
      break;
    }
  }

  for (size_t i = cut_index + 1; i < sz; ++i) {
    blk_list1_.push_back(blk_list_[i]);
  }

  blk_list_.resize(cut_index + 1);
}

void Cluster::UpdateSubClusterSize() {
  p_well_height_ = 0;
  n_well_height_ = 0;
  n_well_height1_ = 0;
  p_well_height1_ = 0;
  for (auto &blk_ptr: blk_list_double_) {
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    int p_well_height = well_ptr->Pheight();
    int n_well_height = well_ptr->Nheight();
    UpdateWellHeightFromBottom(p_well_height, n_well_height);
    int p_well_height1 = well_ptr->Pheight1();
    int n_well_height1 = well_ptr->Nheight1();
    UpdateWellHeightFromBottom1(p_well_height1, n_well_height1);
  }
  for (auto &blk_ptr: blk_list_) {
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    int p_well_height = well_ptr->Pheight();
    int n_well_height = well_ptr->Nheight();
    UpdateWellHeightFromBottom(p_well_height, n_well_height);
  }
  for (auto &blk_ptr: blk_list1_) {
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    int p_well_height = well_ptr->Pheight();
    int n_well_height = well_ptr->Nheight();
    UpdateWellHeightFromBottom1(p_well_height, n_well_height);
  }
}

void Cluster::RedistributeSingleWellCells() {
  for (auto &blk_ptr: blk_list1_) {
    blk_list_.push_back(blk_ptr);
  }
  blk_list1_.clear();
  SplitSingleWellCellList();
  UpdateSubClusterSize();
}

void Cluster::ShiftCellsToCenter() {
  int left_bound = INT_MAX;
  int right_bound = INT_MIN;

  for (auto &blk_ptr: blk_list_double_) {
    left_bound = std::min((int) std::round(blk_ptr->LLX()), left_bound);
    right_bound = std::max((int) std::round(blk_ptr->URX()), right_bound);
  }
  for (auto &blk_ptr: blk_list_) {
    left_bound = std::min((int) std::round(blk_ptr->LLX()), left_bound);
    right_bound = std::max((int) std::round(blk_ptr->URX()), right_bound);
  }
  for (auto &blk_ptr: blk_list1_) {
    left_bound = std::min((int) std::round(blk_ptr->LLX()), left_bound);
    right_bound = std::max((int) std::round(blk_ptr->URX()), right_bound);
  }

  int space_to_left = left_bound - lx_;
  int space_to_right = lx_ + width_ - right_bound;
  int half_space = (space_to_left + space_to_right) / 2;
  int delta_space = half_space - space_to_left;

  for (auto &blk_ptr: blk_list_double_) {
    blk_ptr->IncreaseX(delta_space);
  }
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(delta_space);
  }
  for (auto &blk_ptr: blk_list1_) {
    blk_ptr->IncreaseX(delta_space);
  }
}

void Cluster::UpdateBlockLocYDoubleCluster() {
  for (auto &blk_ptr: blk_list_double_) {
    blk_ptr->SetOrient(N);
  }
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->SetOrient(N);
  }
  for (auto &blk_ptr: blk_list1_) {
    blk_ptr->SetOrient(FS);
    double y_flip_axis = ly_ + ClusterEdge() + height1_ / 2.0;
    double ly_to_axis = y_flip_axis - blk_ptr->LLY();
    blk_ptr->SetURY(y_flip_axis + ly_to_axis);
  }
}

void Cluster::DoubleWellLegalization() {
  // simply legalize cells with a double well
  std::sort(
      blk_list_double_.begin(),
      blk_list_double_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->X() < blk_ptr1->X();
      }
  );
  size_t sz = blk_list_double_.size();
  int contour = lx_;
  for (size_t i = 0; i < sz; ++i) {
    Block *blk_ptr = blk_list_double_[i];
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->SetLLX(contour);
    blk_ptr->SetLLY(ly_ + p_well_height_ - well_ptr->Pheight());
    contour += blk_ptr->Width();
  }

  //RedistributeSingleWellCells();

  // simply legalize cells in sub-cluster 0
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->X() < blk_ptr1->X();
      }
  );
  size_t sz0 = blk_list_.size();
  int contour0 = contour;
  for (size_t i = 0; i < sz0; ++i) {
    Block *blk_ptr = blk_list_[i];
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->SetLLX(contour0);
    blk_ptr->SetLLY(ly_ + p_well_height_ - well_ptr->Pheight());
    contour0 += blk_ptr->Width();
  }

  // simply legalize cells in sub-cluster 1
  std::sort(
      blk_list1_.begin(),
      blk_list1_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->X() < blk_ptr1->X();
      }
  );
  size_t sz1 = blk_list1_.size();
  int contour1 = contour;
  for (size_t i = 0; i < sz1; ++i) {
    Block *blk_ptr = blk_list1_[i];
    BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->SetLLX(contour1);
    blk_ptr->SetLLY(ly_ + ClusterEdge() + p_well_height1_ - well_ptr->Pheight());
    contour1 += blk_ptr->Width();
  }

  ShiftCellsToCenter();
  UpdateBlockLocYDoubleCluster();
}

void ClusterSegment::Merge(
    ClusterSegment &sc,
    int lower_bound,
    int upper_bound
) {
  int sz = (int) sc.cluster_list.size();
  for (int i = 0; i < sz; ++i) {
    cluster_list.push_back(sc.cluster_list[i]);
  }
  height_ += sc.Height();

  sz = (int) cluster_list.size();
  int anchor_size = 0;
  for (auto &cluster_ptr: cluster_list) {
    anchor_size += (int) cluster_ptr->Blocks().size();
  }
  std::vector<double> anchor;
  anchor.reserve(anchor_size);
  int accumulative_d = 0;
  for (int i = 0; i < sz; ++i) {
    for (auto &init_loc: cluster_list[i]->InitLocations()) {
      double init_np_boundary = init_loc.y;
      anchor.push_back(init_np_boundary - accumulative_d);
    }
    accumulative_d += cluster_list[i]->NHeight();
    if (i + 1 < sz) {
      accumulative_d += cluster_list[i + 1]->PHeight();
    } else {
      accumulative_d += cluster_list[0]->PHeight();
    }
  }
  DaliExpects(height_ == accumulative_d,
              "Something is wrong, height does not match");

  long double sum = 0;
  for (auto &num: anchor) {
    sum += num;
  }
  int first_np_boundary = (int) std::round(sum / anchor_size);

  ly_ = first_np_boundary - cluster_list[0]->PHeight();
  if (ly_ < lower_bound) {
    ly_ = lower_bound;
  }
  if (ly_ + height_ > upper_bound) {
    ly_ = upper_bound - height_;
  }
}

void ClusterSegment::UpdateClusterLocation() {
  int cur_y = ly_;
  int sz = (int) cluster_list.size();
  for (int i = 0; i < sz; ++i) {
    cluster_list[i]->SetLLY(cur_y);
    cluster_list[i]->UpdateBlockLocY();
    cur_y += cluster_list[i]->Height();
  }
}

}