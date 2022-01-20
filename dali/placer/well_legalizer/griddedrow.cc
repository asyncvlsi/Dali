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
#include "griddedrow.h"

#include <algorithm>

#include "dali/common/helper.h"

namespace dali {

bool GriddedRow::IsOrientN() const {
  return is_orient_N_;
}

int GriddedRow::UsedSize() const {
  return used_size_;
}

void GriddedRow::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

void GriddedRow::UseSpace(int width) {
  used_size_ += width;
}

void GriddedRow::SetLLX(int lx) {
  lx_ = lx;
}

void GriddedRow::SetURX(int ux) {
  lx_ = ux - width_;
}

int GriddedRow::LLX() const {
  return lx_;
}

int GriddedRow::URX() const {
  return lx_ + width_;
}

double GriddedRow::CenterX() const {
  return lx_ + width_ / 2.0;
}

void GriddedRow::SetWidth(int width) {
  width_ = width;
}

int GriddedRow::Width() const {
  return width_;
}

void GriddedRow::SetLLY(int ly) {
  ly_ = ly;
}

void GriddedRow::SetURY(int uy) {
  ly_ = uy - Height();
}

int GriddedRow::LLY() const {
  return ly_;
}

int GriddedRow::URY() const {
  return ly_ + Height();
}

double GriddedRow::CenterY() const {
  return ly_ + height_ / 2.0;
}

void GriddedRow::SetHeight(int height) {
  height_ = height;
}

/****
* Update the height of this cluster with the lower y of this cluster fixed.
* So even if the height changes, the lower y of this cluster does not need be changed.
* ****/
void GriddedRow::UpdateWellHeightUpward(int p_well_height, int n_well_height) {
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
}

/****
* Update the height of this cluster with the upper y of this cluster fixed.
* So if the height changes, then the lower y of this cluster should also be changed.
* ****/
void GriddedRow::UpdateWellHeightDownward(
    int p_well_height, int n_well_height
) {
  int old_height = height_;
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
  ly_ -= (height_ - old_height);
}

int GriddedRow::Height() const {
  return height_;
}

int GriddedRow::PHeight() const {
  return p_well_height_;
}

int GriddedRow::NHeight() const {
  return n_well_height_;
}

/****
* Returns the P/N well edge to the bottom of this cluster
* ****/
int GriddedRow::PNEdge() const {
  return is_orient_N_ ? PHeight() : NHeight();
}

void GriddedRow::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
}

void GriddedRow::AddBlock(Block *blk_ptr) {
  blk_list_.push_back(blk_ptr);
  double y_init = blk_ptr->LLY();
  BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
  y_init = blk_ptr->LLY() + well_ptr->Pheight();
  blk_initial_location_[blk_ptr] = double2d(blk_ptr->LLX(), y_init);
}

std::vector<Block *> &GriddedRow::Blocks() {
  return blk_list_;
}

std::unordered_map<Block *, double2d> &GriddedRow::InitLocations() {
  return blk_initial_location_;
}

void GriddedRow::ShiftBlockX(int x_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
  }
}

void GriddedRow::ShiftBlockY(int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseY(y_disp);
  }
}

void GriddedRow::ShiftBlock(int x_disp, int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
    blk_ptr->IncreaseY(y_disp);
  }
}

void GriddedRow::UpdateBlockLocY() {
  for (auto &blk_ptr: blk_list_) {
    auto *well = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->SetLLY(ly_ + p_well_height_ - well->Pheight());
  }
}

void GriddedRow::LegalizeCompactX(int left) {
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

void GriddedRow::LegalizeCompactX() {
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

/****
 * Legalize this cluster using the extended Tetris legalization algorithm
 *
 * 1. legalize blocks from left
 * 2. if block contour goes out of the right boundary, legalize blocks from right
 *
 * if the total width of blocks in this cluster is smaller than the width of this cluster,
 * two-rounds legalization is enough to make the final result legal.
 * ****/
void GriddedRow::LegalizeLooseX(int space_to_well_tap) {
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

void GriddedRow::SetOrient(bool is_orient_N) {
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

void GriddedRow::InsertWellTapCell(Block &tap_cell, int loc) {
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

void GriddedRow::UpdateBlockLocationCompact() {
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

void GriddedRow::MinDisplacementLegalization() {
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
    double init_x = blk_initial_location_[blk_ptr].x;
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

void GriddedRow::UpdateMinDisplacementLLY() {
  DaliExpects(blk_list_.size() == blk_initial_location_.size(),
              "Block count does not equal initial location count\n");
  double sum = 0;
  for (auto &[blk_ptr, init_loc]: blk_initial_location_) {
    double init_np_boundary = init_loc.y;
    sum += init_np_boundary;
  }
  min_displacement_lly_ =
      sum / (int) (blk_initial_location_.size()) - PHeight();
}

double GriddedRow::MinDisplacementLLY() const {
  return min_displacement_lly_;
}

std::vector<RowSegment> &GriddedRow::Segments() {
  return segments_;
}

void GriddedRow::UpdateSegments(
    std::vector<SegI> &blockage,
    bool is_existing_blks_considered
) {
  // collect used space segments
  std::vector<SegI> used_spaces = blockage;

  // treat existing blocks as blockages
  if (is_existing_blks_considered) {
    for (auto &blk_region: blk_regions_) {
      Block *p_blk = blk_region.p_blk;
      used_spaces.emplace_back(p_blk->LLX(), p_blk->URX());
    }
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
  DaliExpects((len & 1) == 0, "odd number of segments? VERY ODD!");
  segments_.clear();
  segments_.reserve(len / 2);
  for (size_t i = 0; i < len; i += 2) {
    segments_.emplace_back();
    RowSegment &segment = segments_.back();
    segment.SetLLX(intermediate_seg[i]);
    segment.SetWidth(intermediate_seg[i + 1] - intermediate_seg[i]);
  }
}

/****
 * @brief: re-assign blocks to row segments
 */
void GriddedRow::AssignBlocksToSegments() {
  for (auto &[p_blk, region_id]: blk_regions_) {
    bool is_completely_in_a_seg = false;
    for (auto &seg: segments_) {
      if (p_blk->LLX() >= seg.LLX() &&
          p_blk->URX() <= seg.URX()) {
        double2d &init_loc = blk_initial_location_[p_blk];
        seg.AddBlockRegion(p_blk, region_id);
        is_completely_in_a_seg = true;
        break;
      }
    }
    DaliExpects(is_completely_in_a_seg,
                "A block is not completely in any row segment?!!");
  }
}

bool GriddedRow::IsBelowMiddleLine(Block *p_blk) const {
  return p_blk->LLY() < CenterY();
}

bool GriddedRow::IsBelowTopPlusKFirstRegionHeight(
    Block *p_blk, int iteration
) const {
  return p_blk->LLY() < URY() + height_ * (iteration - 1);
}

bool GriddedRow::IsAboveMiddleLine(Block *p_blk) const {
  return p_blk->URY() > CenterY();
}

bool GriddedRow::IsAboveBottomMinusKFirstRegionHeight(
    Block *p_blk, int iteration
) const {
  return p_blk->URY() > LLY() - height_ * (iteration - 1);
}

bool GriddedRow::IsOverlap(
    Block *p_blk,
    int iteration,
    bool is_upward
) const {
  if (is_upward) {
    if (iteration > 0) {
      return IsBelowTopPlusKFirstRegionHeight(p_blk, iteration);
    } else {
      return IsBelowMiddleLine(p_blk);
    }
  } else {
    if (iteration > 0) {
      return IsAboveBottomMinusKFirstRegionHeight(p_blk, iteration);
    } else {
      return IsAboveMiddleLine(p_blk);
    }
  }
}

bool GriddedRow::IsOrientMatching(Block *p_blk, int region_id) const {
  BlockTypeWell *well_ptr = p_blk->TypePtr()->WellPtr();
  // cells with an odd number of regions can be fitted into any clusters
  if (well_ptr->HasOddRegions()) {
    return true;
  }
  // cells with an even number of regions can only be fitted into clusters with the same well orientation
  return IsOrientN() ? well_ptr->IsNwellAbovePwell(region_id)
                     : !well_ptr->IsNwellAbovePwell(region_id);
}

void GriddedRow::AddBlockRegion(Block *p_blk, int region_id, bool is_upward) {
  blk_regions_.emplace_back(p_blk, region_id);
  BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
  int p_height = well->PwellHeight(region_id, p_blk->IsFlipped());
  int n_height = well->NwellHeight(region_id, p_blk->IsFlipped());
  if (is_upward) {
    UpdateWellHeightUpward(p_height, n_height);
  } else {
    UpdateWellHeightDownward(p_height, n_height);
  }
}

bool GriddedRow::AttemptToAdd(Block *p_blk, bool is_upward) {
  // put this block to the closest white space segment
  double min_distance = DBL_MAX;
  int min_index = -1;
  int sz = static_cast<int>(segments_.size());
  for (int i = 0; i < sz; ++i) {
    auto &segment = segments_[i];
    double distance = DBL_MAX;
    if (segment.UsedSize() + p_blk->Width() <= segment.Width()) {
      if (p_blk->LLX() >= segment.LLX()
          && p_blk->URX() <= segment.URX()) {
        distance = 0;
      } else {
        distance = std::min(
            std::fabs(p_blk->LLX() - segment.LLX()),
            std::fabs(p_blk->URX() - segment.URX())
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

  int region_count = p_blk->TypePtr()->WellPtr()->RegionCount();
  int region_id = is_upward ? 0 : region_count - 1;
  segments_[min_index].AddBlockRegion(p_blk, region_id);
  p_blk->SetOrient(ComputeBlockOrient(p_blk, is_upward));
  AddBlockRegion(p_blk, region_id, is_upward);

  return true;
}

BlockOrient GriddedRow::ComputeBlockOrient(Block *p_blk, bool is_upward) const {
  DaliExpects(p_blk != nullptr, "Nullptr?");
  BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
  BlockOrient orient = N;
  int region_count = p_blk->TypePtr()->WellPtr()->RegionCount();
  int region_id = is_upward ? 0 : region_count - 1;
  bool is_cluster_block_orientation_matching =
      (is_orient_N_ && well->IsNwellAbovePwell(region_id)) ||
          (!is_orient_N_ && !well->IsNwellAbovePwell(region_id));
  if (!is_cluster_block_orientation_matching) {
    orient = FS;
  }
  return orient;
}

void GriddedRow::LegalizeSegmentsX() {
  for (auto &segment: segments_) {
    segment.MinDisplacementLegalization();
  }
}

void GriddedRow::LegalizeSegmentsY() {
  for (auto &[p_blk, region_id]: blk_regions_) {
    if (region_id != 0) continue;
    BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
    double y_loc = LLY();
    if (is_orient_N_) {
      y_loc += p_well_height_ - well->PwellHeight(0, p_blk->IsFlipped());
    } else {
      y_loc += n_well_height_ - well->NwellHeight(0, p_blk->IsFlipped());
    }
    p_blk->SetLLY(y_loc);
  }
}

void GriddedRow::RecomputeHeight(int p_well_height, int n_well_height) {
  p_well_height_ = p_well_height;
  n_well_height_ = n_well_height;
  for (auto &blk_region: blk_regions_) {
    auto *p_blk = blk_region.p_blk;
    int region_id = blk_region.region_id;
    BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
    int p_height = well->PwellHeight(region_id, p_blk->IsFlipped());
    int n_height = well->NwellHeight(region_id, p_blk->IsFlipped());
    p_well_height_ = std::max(p_well_height_, p_height);
    n_well_height_ = std::max(n_well_height_, n_height);
  }
  height_ = p_well_height_ + n_well_height_;
}

void GriddedRow::InitializeBlockStretching() {
  for (auto &blk_region: blk_regions_) {
    auto *p_blk = blk_region.p_blk;
    size_t region_id = blk_region.region_id;
    BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
    size_t row_cnt = well->RegionCount();
    if (region_id == 0) {
      p_blk->StretchLengths().resize(row_cnt - 1, 0);
    }
  }
}

size_t GriddedRow::AddWellTapCells(
    Circuit *p_ckt,
    BlockType *well_tap_type_ptr,
    size_t start_id,
    std::vector<SegI> &well_tap_cell_locs
) {
  auto &tap_cell_list = p_ckt->design().WellTaps();
  BlockTypeWell *well = well_tap_type_ptr->WellPtr();
  double y_loc = LLY();
  if (is_orient_N_) {
    y_loc += p_well_height_ - well->PwellHeight(0, false);
  } else {
    y_loc += n_well_height_ - well->NwellHeight(0, true);
  }
  BlockOrient orient = is_orient_N_ ? N : FS;
  for (auto &[lo_x, hi_x]: well_tap_cell_locs) {
    std::string block_name = "__well_tap__" + std::to_string(start_id++);
    tap_cell_list.emplace_back();
    auto &tap_cell = tap_cell_list.back();
    tap_cell.SetPlacementStatus(PLACED);
    tap_cell.SetType(well_tap_type_ptr);
    int map_size = static_cast<int>(p_ckt->design().TapNameIdMap().size());
    auto ret = p_ckt->design().TapNameIdMap().insert(
        std::pair<std::string, int>(block_name, map_size)
    );
    auto *name_id_pair_ptr = &(*ret.first);
    tap_cell.SetNameNumPair(name_id_pair_ptr);
    tap_cell.SetLLX(lo_x);
    tap_cell.SetLLY(y_loc);
    tap_cell.SetOrient(orient);
  }
  return start_id;
}

/****
 * @brief sort blocks in this row based on their x location
 *
 * If two cells have the same x location, then sort them based on their index.
 */
void GriddedRow::SortBlockRegions() {
  std::sort(
      blk_regions_.begin(),
      blk_regions_.end(),
      [](const BlockRegion r0, const BlockRegion r1) {
        return (r0.p_blk->LLX() < r1.p_blk->LLX()) ||
            ((r0.p_blk->LLX() == r1.p_blk->LLX())
                && (r0.p_blk->Id() < r1.p_blk->Id()));
      }
  );
}

bool GriddedRow::IsRowLegal() {
  SortBlockRegions();
  int front = LLX();
  for (BlockRegion &blk_region: blk_regions_) {
    Block *blk_ptr = blk_region.p_blk;
    int blk_lx = static_cast<int>(std::round(blk_ptr->LLX()));
    if (blk_lx < front) return false;
    front += blk_ptr->Width();
  }
  return front <= URX();
}

void ClusterSegment::Merge(
    ClusterSegment &sc,
    int lower_bound,
    int upper_bound
) {
  int sz = (int) sc.gridded_rows.size();
  for (int i = 0; i < sz; ++i) {
    gridded_rows.push_back(sc.gridded_rows[i]);
  }
  height_ += sc.Height();

  sz = (int) gridded_rows.size();
  int anchor_size = 0;
  for (auto &cluster_ptr: gridded_rows) {
    anchor_size += (int) cluster_ptr->Blocks().size();
  }
  std::vector<double> anchor;
  anchor.reserve(anchor_size);
  int accumulative_d = 0;
  for (int i = 0; i < sz; ++i) {
    for (auto &[blk_ptr, init_loc]: gridded_rows[i]->InitLocations()) {
      double init_np_boundary = init_loc.y;
      anchor.push_back(init_np_boundary - accumulative_d);
    }
    accumulative_d += gridded_rows[i]->NHeight();
    if (i + 1 < sz) {
      accumulative_d += gridded_rows[i + 1]->PHeight();
    } else {
      accumulative_d += gridded_rows[0]->PHeight();
    }
  }
  DaliExpects(height_ == accumulative_d,
              "Something is wrong, height does not match");

  long double sum = 0;
  for (auto &num: anchor) {
    sum += num;
  }
  int first_np_boundary = (int) std::round(sum / anchor_size);

  ly_ = first_np_boundary - gridded_rows[0]->PHeight();
  if (ly_ < lower_bound) {
    ly_ = lower_bound;
  }
  if (ly_ + height_ > upper_bound) {
    ly_ = upper_bound - height_;
  }
}

void ClusterSegment::UpdateClusterLocation() {
  int cur_y = ly_;
  int sz = (int) gridded_rows.size();
  for (int i = 0; i < sz; ++i) {
    gridded_rows[i]->SetLLY(cur_y);
    gridded_rows[i]->UpdateBlockLocY();
    cur_y += gridded_rows[i]->Height();
  }
}

}