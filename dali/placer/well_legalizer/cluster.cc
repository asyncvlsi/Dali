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

#include "dali/common/helper.h"

namespace dali {

bool Cluster::IsOrientN() const {
  return is_orient_N_;
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
  return height_;
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
  for (auto &seg: segments) {
    seg.UpdateBlockLocation();
    //count += seg.blk_list.size();
    //seg.Report();
  }
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

void Cluster::UpdateSubClusters() {
  // collect used space segments
  std::vector<SegI> used_spaces;
  for (auto &blk_region: blk_regions_) {
    Block *p_blk = blk_region.p_blk;
    used_spaces.emplace_back(p_blk->LLX(), p_blk->URX());
  }

  MergeIntervals(used_spaces);

  // collect unused space segments
  std::vector<int> intermediate_seg;
  if (used_spaces.empty()) {
    intermediate_seg.push_back(LLX());
    intermediate_seg.push_back(URX());
  } else {
    size_t segments_size = used_spaces.size();
    for (size_t i = 0; i < segments_size; ++i) {
      auto &interval = used_spaces[i];
      if (interval.lo == LLX() && interval.hi < URX()) {
        intermediate_seg.push_back(interval.hi);
      }

      if (interval.lo > LLX()) {
        if (intermediate_seg.empty()) {
          intermediate_seg.push_back(LLX());
        }
        intermediate_seg.push_back(interval.lo);
        if (interval.hi < URX()) {
          intermediate_seg.push_back(interval.hi);
        }
      }
    }
    if (intermediate_seg.size() % 2 == 1) {
      intermediate_seg.push_back(URX());
    }
  }

  // create sub-clusters
  size_t len = intermediate_seg.size();
  sub_clusters_.clear();
  sub_clusters_.reserve(len / 2);
  for (size_t i = 0; i < len; i += 2) {
    sub_clusters_.emplace_back();
    Cluster &sub_cluster = sub_clusters_.back();
    sub_cluster.SetLLX(intermediate_seg[i]);
    sub_cluster.SetWidth(intermediate_seg[i + 1] - intermediate_seg[i]);
    sub_cluster.SetOrient(is_orient_N_);
    sub_cluster.SetLLY(ly_);
  }
}

bool Cluster::IsBelowTopBoundary(Block *p_blk) const {
  return p_blk->LLY() < URY();
}

bool Cluster::IsBelowMiddleLine(Block *p_blk) const {
  return p_blk->LLY() < CenterY();
}

bool Cluster::IsOverlap(Block *p_blk, int criterion) const {
  switch (criterion) {
    case 0: {
      return IsBelowTopBoundary(p_blk);
    }
    case 1: {
      return IsBelowMiddleLine(p_blk);
    }
    default: {
      DaliExpects(false, "unknown overlapping criterion");
      return false;
    }
  }
}

bool Cluster::IsOrientMatching(Block *p_blk) const {
  BlockTypeMultiWell *p_well = p_blk->TypePtr()->MultiWellPtr();
  // cells with an odd number of regions can be fitted into any clusters
  if (p_well->HasOddRegions()) {
    return true;
  }
  // cells with an even number of regions can only be fitted into clusters with the same well orientation
  return IsOrientN() ? p_well->IsBottomWellP() : !p_well->IsBottomWellP();
}

void Cluster::AddBlockRegion(Block *p_blk, size_t region_id) {
  blk_regions_.emplace_back(p_blk, region_id);
  BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
  int p_height = well->PwellHeight(region_id, p_blk->IsFlipped());
  int n_height = well->NwellHeight(region_id, p_blk->IsFlipped());
  p_well_height_ = std::max(p_well_height_, p_height);
  n_well_height_ = std::max(n_well_height_, n_height);
  height_ = p_well_height_ + n_well_height_;
}

bool Cluster::AttemptToAdd(Block *p_blk) {
  // put this block to the closest white space segment
  double min_distance = DBL_MAX;
  int min_index = -1;
  int sz = static_cast<int>(sub_clusters_.size());
  for (int i = 0; i < sz; ++i) {
    auto &sub_cluster = sub_clusters_[i];
    double distance = DBL_MAX;
    if (sub_cluster.used_size_ + p_blk->Width() <= sub_cluster.width_) {
      if (p_blk->LLX() >= sub_cluster.LLX()
          && p_blk->URX() <= sub_cluster.URX()) {
        distance = 0;
      } else {
        distance = std::min(
            std::fabs(p_blk->LLX() - sub_cluster.LLX()),
            std::fabs(p_blk->URX() - sub_cluster.URX())
        );
      }
    }
    if (distance < min_distance) {
      min_distance = distance;
      min_index = i;
    }
  }

  if (min_index == -1) {
    return false;
  }

  sub_clusters_[min_index].blk_list_.push_back(p_blk);
  sub_clusters_[min_index].used_size_ += p_blk->Width();
  p_blk->SetOrient(ComputeOrient(p_blk));
  AddBlockRegion(p_blk, 0);
  return true;
}

BlockOrient Cluster::ComputeOrient(Block *p_blk) {
  DaliExpects(p_blk != nullptr, "Nullptr?");
  BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
  BlockOrient orient = N;
  bool is_cluster_block_orientation_matching =
      (is_orient_N_ && well->IsBottomWellP()) ||
          (!is_orient_N_ && !well->IsBottomWellP());
  if (!is_cluster_block_orientation_matching) {
    orient = FS;
  }
  return orient;
}

void Cluster::SubClusterLegalize() {
  for (auto &sub_cluster: sub_clusters_) {
    sub_cluster.LegalizeCompactX();

    for (auto &p_blk: sub_cluster.blk_list_) {
      BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
      if (is_orient_N_) {
        double y_loc = sub_cluster.LLY() + p_well_height_
            - well->PwellHeight(0, p_blk->IsFlipped());
        p_blk->SetLLY(y_loc);
      } else {
        double y_loc = sub_cluster.LLY() + n_well_height_
            - well->NwellHeight(0, !p_blk->IsFlipped());
        p_blk->SetLLY(y_loc);
      }
    }

  }
}

void Cluster::RecomputeHeight(int p_well_height, int n_well_height) {
  p_well_height_ = p_well_height;
  n_well_height_ = n_well_height;
  for (auto &blk_region: blk_regions_) {
    auto *p_blk = blk_region.p_blk;
    size_t region_id = blk_region.region_id;
    BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
    int p_height = well->PwellHeight(region_id, p_blk->IsFlipped());
    int n_height = well->NwellHeight(region_id, p_blk->IsFlipped());
    p_well_height_ = std::max(p_well_height_, p_height);
    n_well_height_ = std::max(n_well_height_, n_height);
  }
  height_ = p_well_height_ + n_well_height_;
}

void Cluster::InitializeBlockStretching() {
  for (auto &blk_region: blk_regions_) {
    auto *p_blk = blk_region.p_blk;
    size_t region_id = blk_region.region_id;
    BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
    size_t row_cnt = well->RowCount();
    if (region_id == 0) {
      p_blk->StretchLengths().resize(row_cnt-1, 0);
    }
  }
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