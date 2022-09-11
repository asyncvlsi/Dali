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
#include "stripe.h"

#include <omp.h>

#include <algorithm>

#include "dali/placer/well_legalizer/blockhelper.h"
#include "dali/placer/well_legalizer/lgblkaux.h"
#include "dali/placer/well_legalizer/stripehelper.h"

namespace dali {

bool Stripe::HasNoRowsSpillingOut() const {
  if (gridded_rows_.empty()) return true;

  // check if the first row fully inside the placement region
  bool is_first_row_fully_in = gridded_rows_[0].LLY() >= LLY() &&
      gridded_rows_[0].URY() <= URY();
  // check if the last row fully inside the placement region
  bool is_last_row_fully_in = gridded_rows_.back().LLY() >= LLY() &&
      gridded_rows_.back().URY() <= URY();

  return is_first_row_fully_in && is_last_row_fully_in;
}

void Stripe::MinDisplacementAdjustment() {
  for (auto &row : gridded_rows_) {
    row.UpdateMinDisplacementLLY();
  }

  std::sort(
      gridded_rows_.begin(),
      gridded_rows_.end(),
      [](const GriddedRow &cluster0, const GriddedRow &cluster1) {
        return cluster0.MinDisplacementLLY()
            < cluster1.MinDisplacementLLY();
      }
  );

  std::vector<ClusterSegment> segments;

  int32_t sz = (int32_t) gridded_rows_.size();
  int32_t lower_bound = ly_;
  int32_t upper_bound = ly_ + height_;
  for (int32_t i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    GriddedRow &row = gridded_rows_[i];
    double init_y = row.MinDisplacementLLY();
    if (init_y < lower_bound) {
      init_y = lower_bound;
    }
    if (init_y + row.Height() > upper_bound) {
      init_y = upper_bound - row.Height();
    }
    segments.emplace_back(&row, init_y);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    ClusterSegment *cur_seg = &(segments[seg_sz - 1]);
    ClusterSegment *prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnBottom(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  for (auto &seg : segments) {
    seg.UpdateClusterLocation();
  }
}

void Stripe::SortBlocksBasedOnLLY() {
  std::sort(
      blk_ptrs_vec_.begin(),
      blk_ptrs_vec_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->LLY() < rhs->LLY())
            || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnURY() {
  std::sort(
      blk_ptrs_vec_.begin(),
      blk_ptrs_vec_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->URY() < rhs->URY())
            || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnStretchedURY() {
  std::sort(
      blk_ptrs_vec_.begin(),
      blk_ptrs_vec_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->URY() > rhs->URY())
            || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnYLocation(int32_t criterion) {
  switch (criterion) {
    case 0: {
      SortBlocksBasedOnLLY();
      break;
    }
    case 1: {
      SortBlocksBasedOnURY();
      break;
    }
    case 2: {
      SortBlocksBasedOnStretchedURY();
      break;
    }
    default: {
      DaliExpects(false, "unknown block sorting criterion");
    }
  }
}

void Stripe::PrecomputeWellTapCellLocation(
    bool is_checker_board_mode,
    int32_t tap_cell_interval_grid,
    BlockType *well_tap_type_ptr
) {
  is_checkerboard_mode_ = is_checker_board_mode;
  DaliExpects(tap_cell_interval_grid > 0,
              "Non-positive well-tap cell interval?");
  DaliExpects(well_tap_type_ptr != nullptr, "Well-tap cell is a nullptr?");
  well_tap_cell_width_ = well_tap_type_ptr->Width();
  DaliExpects(width_ > well_tap_cell_width_,
              "Stripe width is smaller than well-tap cell width?");

  std::vector<int32_t> locations;

  if (is_checkerboard_mode_) {
    if (tap_cell_interval_grid & 1) { // if this interval is an odd number
      int32_t new_tap_cell_interval_grid = tap_cell_interval_grid - 1;
      BOOST_LOG_TRIVIAL(info)
        << "Rounding well tap cell interval from " << tap_cell_interval_grid
        << " to " << new_tap_cell_interval_grid << "\n";
      tap_cell_interval_grid = new_tap_cell_interval_grid;
    }
    tap_cell_interval_grid = tap_cell_interval_grid / 2;
  }

  // compute how many well-tap cells are needed
  int32_t space_to_left = 0; // TODO: this can be a parameter exposed to users
  int32_t well_tap_cell_loc = lx_ + space_to_left;
  int32_t number_of_well_tap_cell =
      (width_ - space_to_left) / tap_cell_interval_grid + 1;
  for (int32_t i = 0; i < number_of_well_tap_cell; ++i) {
    locations.emplace_back(well_tap_cell_loc);
    well_tap_cell_loc += tap_cell_interval_grid;
  }

  // check if the ux of the last well-tap cell is out of the region
  int32_t last_ux = locations.back() + well_tap_cell_width_;
  int32_t right_most_loc = URX() - well_tap_cell_width_;
  if (last_ux > URX()) {
    locations.back() = right_most_loc;
  } else {
    // check if the last segment is longer than half of the well tap cell interval
    if (URX() - last_ux >= tap_cell_interval_grid / 2.0) {
      locations.emplace_back(right_most_loc);
    }
  }

  // finalize well-tap cell locations
  if (is_checkerboard_mode_) {
    size_t sz = locations.size();
    size_t half_sz = (sz + 1) >> 1; // divide by 2
    well_tap_cell_location_even_.reserve(half_sz);
    well_tap_cell_location_odd_.reserve(half_sz);
    for (size_t i = 0; i < sz; ++i) {
      int32_t lo_loc = locations[i];
      int32_t hi_loc = locations[i] + well_tap_cell_width_;
      if (i & 1) { // if index i is an odd number
        well_tap_cell_location_odd_.emplace_back(lo_loc, hi_loc);
      } else {
        well_tap_cell_location_even_.emplace_back(lo_loc, hi_loc);
      }
    }
  } else {
    size_t sz = locations.size();
    well_tap_cell_location_even_.reserve(sz);
    well_tap_cell_location_odd_.reserve(sz);
    for (size_t i = 0; i < sz; ++i) {
      int32_t lo_loc = locations[i];
      int32_t hi_loc = locations[i] + well_tap_cell_width_;
      well_tap_cell_location_odd_.emplace_back(lo_loc, hi_loc);
      well_tap_cell_location_even_.emplace_back(lo_loc, hi_loc);
    }
  }
}

void Stripe::UpdateFrontClusterUpward(int32_t p_height, int32_t n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int32_t>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the lower y location of the front cluster
  int32_t ly;
  bool is_orient_N;
  if (front_id_ == 0) {
    ly = LLY();
    is_orient_N = is_first_row_orient_N_;
  } else {
    ly = gridded_rows_[front_id_ - 1].URY();
    is_orient_N = !gridded_rows_[front_id_ - 1].IsOrientN();
  }
  gridded_rows_[front_id_].SetLLY(ly);
  gridded_rows_[front_id_].SetOrient(is_orient_N);

  gridded_rows_[front_id_].UpdateWellHeightUpward(p_height, n_height);
  if (front_id_ & 1) {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_odd_, true);
  } else {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_even_, true);
  }
}

/****
 * Add following clusters and set orientation accordingly.
 * Add the corresponding block region into these clusters based on row orientation and block orientation.
 *
 * @param p_blk
 */
void Stripe::SimplyAddFollowingClusters(Block *p_blk, bool is_upward) {
  BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
  int32_t region_count = well->RegionCount();
  for (int32_t i = 1; i < region_count; ++i) {
    int32_t row_index = front_id_ + i;
    if (row_index >= static_cast<int32_t>(gridded_rows_.size())) {
      gridded_rows_.emplace_back();
    }
    bool is_orient_N = !gridded_rows_[row_index - 1].IsOrientN();
    gridded_rows_[row_index].SetOrient(is_orient_N);
    int32_t region_id = is_upward ? i : region_count - 1 - i;
    gridded_rows_[row_index].AddBlockRegion(p_blk, region_id, true);
  }
}

bool Stripe::AddBlockToFrontCluster(Block *p_blk, bool is_upward) {
  bool res = gridded_rows_[front_id_].AttemptToAdd(p_blk, is_upward);
  if (!res) return false;

  // add this block to other clusters above the front cluster
  SimplyAddFollowingClusters(p_blk, is_upward);

  return true;
}

bool Stripe::AddBlockToFrontClusterWithDispCheck(
    Block *p_blk,
    double displacement_upper_limit,
    bool is_upward
) {
  bool res = gridded_rows_[front_id_].AttemptToAddWithDispCheck(
      p_blk,
      displacement_upper_limit,
      is_upward
  );
  if (!res) return false;

  // add this block to other clusters above the front cluster
  SimplyAddFollowingClusters(p_blk, is_upward);

  return true;
}

size_t Stripe::FitBlocksToFrontSpaceUpward(
    size_t start_id,
    int32_t current_iteration
) {
  std::vector<Block *> legalized_blocks;
  std::vector<Block *> skipped_blocks;

  size_t blocks_sz = blk_ptrs_vec_.size();
  for (size_t i = start_id; i < blocks_sz; ++i) {
    Block *p_blk = blk_ptrs_vec_[i];
    if (!gridded_rows_[front_id_].IsOverlap(p_blk, current_iteration, true)) {
      break;
    }
    if (gridded_rows_[front_id_].IsOrientMatching(p_blk, 0)) {
      if (AddBlockToFrontCluster(p_blk, true)) {
        legalized_blocks.push_back(p_blk);
      } else {
        skipped_blocks.push_back(p_blk);
      }
    } else {
      skipped_blocks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = legalized_blocks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blocks.size();
  for (size_t i = 0; i < skipped_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = skipped_blocks[i];
  }

  return start_id;
}

size_t Stripe::FitBlocksToFrontSpaceUpwardWithDispCheck(
    size_t start_id, double displacement_upper_limit
) {
  std::vector<Block *> legalized_blocks;
  std::vector<Block *> skipped_blocks;

  size_t blocks_sz = blk_ptrs_vec_.size();
  for (size_t i = start_id; i < blocks_sz; ++i) {
    Block *p_blk = blk_ptrs_vec_[i];
    if (gridded_rows_[front_id_].IsOrientMatching(p_blk, 0)) {
      if (AddBlockToFrontClusterWithDispCheck(p_blk,
                                              displacement_upper_limit,
                                              true)) {
        legalized_blocks.push_back(p_blk);
      } else {
        skipped_blocks.push_back(p_blk);
      }
    } else {
      skipped_blocks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = legalized_blocks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blocks.size();
  for (size_t i = 0; i < skipped_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = skipped_blocks[i];
  }

  return start_id;
}

void Stripe::LegalizeFrontCluster(bool use_init_loc) {
  gridded_rows_[front_id_].LegalizeSegmentsX(use_init_loc);
}

void Stripe::UpdateRemainingClusters(
    int32_t p_height,
    int32_t n_height,
    bool is_upward
) {
  size_t sz = gridded_rows_.size();
  for (size_t i = front_id_ + 1; i < sz; ++i) {
    gridded_rows_[i].SetLLX(lx_);
    gridded_rows_[i].SetWidth(width_);
    gridded_rows_[i].RecomputeHeight(p_height, n_height);
    if (is_upward) {
      gridded_rows_[i].SetLLY(gridded_rows_[i - 1].URY());
    } else {
      gridded_rows_[i].SetURY(gridded_rows_[i - 1].LLY());
    }
  }
}

void Stripe::UpdateBlockStretchLength() {
  if (!is_bottom_up_) {
    std::reverse(gridded_rows_.begin(), gridded_rows_.end());
    is_bottom_up_ = true;
  }

  for (auto &row : gridded_rows_) {
    row.InitializeBlockStretching();
  }

  int32_t sz = static_cast<int32_t>(gridded_rows_.size());
  for (int32_t i = 1; i < sz; ++i) {
    GriddedRow &cur_cluster = gridded_rows_[i];
    GriddedRow &pre_cluster = gridded_rows_[i - 1];
    for (auto &blk_region : cur_cluster.blk_regions_) {
      int32_t id = blk_region.region_id;
      if (id >= 1) {
        Block *p_blk = blk_region.p_blk;
        BlockTypeWell *well = blk_region.p_blk->TypePtr()->WellPtr();
        --id;
        int32_t well_edge_distance =
            well->AdjacentRegionEdgeDistance(id, p_blk->IsFlipped());
        int32_t actual_edge_distance = (cur_cluster.LLY() + cur_cluster.PNEdge()) -
            (pre_cluster.LLY() + pre_cluster.PNEdge());
        int32_t length = actual_edge_distance - well_edge_distance;
        p_blk->SetStretchLength(id, length);
      }
    }
  }
}

void Stripe::UpdateFrontClusterDownward(int32_t p_height, int32_t n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int32_t>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the upper y of the front cluster
  int32_t uy;
  bool is_orient_N;
  if (front_id_ == 0) {
    uy = URY();
    if (blk_ptrs_vec_.empty()) {
      is_orient_N = is_first_row_orient_N_;
    } else {
      BlockTypeWell *well_ptr = blk_ptrs_vec_[0]->TypePtr()->WellPtr();
      bool is_blk_flipped = blk_ptrs_vec_[0]->IsFlipped();
      if (is_blk_flipped) {
        is_orient_N = !well_ptr->IsNwellAbovePwell(0);
      } else {
        int32_t region_id = well_ptr->RegionCount() - 1;
        is_orient_N = well_ptr->IsNwellAbovePwell(region_id);
      }
    }
  } else {
    uy = gridded_rows_[front_id_ - 1].LLY();
    is_orient_N = !gridded_rows_[front_id_ - 1].IsOrientN();
  }
  gridded_rows_[front_id_].SetURY(uy);
  gridded_rows_[front_id_].SetOrient(is_orient_N);
  gridded_rows_[front_id_].UpdateWellHeightDownward(p_height, n_height);
  if (front_id_ & 1) {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_odd_, true);
  } else {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_even_, true);
  }
}

size_t Stripe::FitBlocksToFrontSpaceDownward(
    size_t start_id,
    int32_t current_iteration
) {
  std::vector<Block *> legalized_blocks;
  std::vector<Block *> skipped_blocks;

  size_t blocks_sz = blk_ptrs_vec_.size();
  for (size_t i = start_id; i < blocks_sz; ++i) {
    Block *p_blk = blk_ptrs_vec_[i];
    if (!gridded_rows_[front_id_].IsOverlap(p_blk, current_iteration, false)) {
      break;
    }
    int32_t region_id = p_blk->TypePtr()->WellPtr()->RegionCount() - 1;
    if (gridded_rows_[front_id_].IsOrientMatching(p_blk, region_id)) {
      if (AddBlockToFrontCluster(p_blk, false)) {
        legalized_blocks.push_back(p_blk);
      } else {
        skipped_blocks.push_back(p_blk);
      }
    } else {
      skipped_blocks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = legalized_blocks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blocks.size();
  for (size_t i = 0; i < skipped_blocks.size(); ++i) {
    blk_ptrs_vec_[i + start_id] = skipped_blocks[i];
  }

  return start_id;
}

/****
 * @brief Update the Y location of all cells
 */
void Stripe::UpdateBlockYLocation() {
  for (GriddedRow &row : gridded_rows_) {
    row.LegalizeSegmentsY();
  }
}

/****
 * @brief Clean up temporary row segments
 *
 * This is a special method for the greedy displacement optimization algorithm.
 * In this algorithm, the location of a multi-deck cell will be determined when
 * it first assigned to a row, and then this cell is viewed as a fixed blockage,
 * which means it will cut several subsequent row segments into smaller row
 * segments. But these additional segments are not really necessary: they are
 * only for simplifying the code implementation. Therefore, we need to clean up
 * these temporary row segments once the greedy legalization is done.
 *
 * Temporary row segments are created by the greedy legalization algorithm fore
 * simplifying the code implementation. These temporary row segments are not
 * real row segments, and thus they need to be cleaned up in order not to confuse
 * following processes.
 *
 */
void Stripe::CleanUpTemporaryRowSegments() {
  int32_t row_cnt = static_cast<int32_t>(gridded_rows_.size());
  for (int32_t i = 0; i < row_cnt; ++i) {
    GriddedRow &row = gridded_rows_[i];
    // remove all temporary and intrinsic segments
    row.Segments().clear();
    // re-create intrinsic segments
    if (i & 1) {
      row.UpdateSegments(well_tap_cell_location_odd_, false);
    } else {
      row.UpdateSegments(well_tap_cell_location_even_, false);
    }
    // assign blocks back to row segments
    row.AssignBlocksToSegments();
  }
}

size_t Stripe::AddWellTapCells(
    Circuit *p_ckt,
    BlockType *well_tap_type_ptr,
    size_t start_id
) {
  size_t row_cnt = gridded_rows_.size();
  for (size_t i = 0; i < row_cnt; ++i) {
    if (i & 1) {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_type_ptr, start_id, well_tap_cell_location_odd_
      );
    } else {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_type_ptr, start_id, well_tap_cell_location_even_
      );
    }
  }
  return start_id;
}

bool Stripe::IsLeftmostPlacementLegal() {
  std::sort(
      blk_ptrs_vec_.begin(),
      blk_ptrs_vec_.end(),
      [](const Block *blk0, const Block *blk1) {
        return (blk0->LLX() < blk1->LLX()) ||
            ((blk0->LLX() == blk1->LLX()) && (blk0->Id() < blk1->Id()));
      }
  );

  DaliExpects(false, "To be implemented");
  //for (Block *&blk : blk_ptrs_vec_) {
  //}

  return true;
}

bool Stripe::IsStripeLegal() {
  for (GriddedRow &row : gridded_rows_) {
    if (!row.IsRowLegal()) return false;
  }
  return true;
}

void Stripe::CollectAllRowSegments() {
  size_t row_seg_cnt = 0;
  for (GriddedRow &row : gridded_rows_) {
    row_seg_cnt += row.segments_.size();
  }
  row_seg_ptrs_.reserve(row_seg_cnt);
  for (GriddedRow &row : gridded_rows_) {
    for (RowSegment &segment : row.segments_) {
      row_seg_ptrs_.push_back(&segment);
    }
  }
}

void Stripe::UpdateSubCellLocs(std::vector<BlkDispVar> &vars) {
  for (BlkDispVar &var : vars) {
    Block *blk_ptr = var.blk_rgn.p_blk;
    if (blk_ptr == nullptr) continue; // skip dummy cells
    auto *aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    aux_ptr->SetSubCellLoc(
        var.blk_rgn.region_id,
        var.Solution(),
        var.SegmentWeight()
    );
  }
}

void Stripe::OptimizeDisplacementInEachRowSegment(
    double lambda,
    bool is_weighted_anchor,
    bool is_reorder
) {
  size_t sz = row_seg_ptrs_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    RowSegment *seg = row_seg_ptrs_[i];
    std::vector<BlkDispVar> vars =
        seg->OptimizeQuadraticDisplacement(lambda,
                                           is_weighted_anchor,
                                           is_reorder);
    //std::vector<BlkDispVar> vars =
    //    seg->OptimizeLinearDisplacement(lambda, is_weighted_anchor, is_reorder);
    UpdateSubCellLocs(vars);
  }
}

void Stripe::ComputeAverageLoc() {
  size_t sz = blk_ptrs_vec_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    Block *blk_ptr = blk_ptrs_vec_[i];
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    aux_ptr->ComputeAverageLoc();
    blk_ptr->SetLLX(aux_ptr->AverageLoc());
  }
}

void Stripe::ReportIterativeStatus(int32_t i) {
  double disp_x = 0;
  double discrepancy = 0;
  max_discrepancy_ = 0;
  for (auto &blk_ptr : blk_ptrs_vec_) {
    // compute displacement from init_x to average_x
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(aux_ptr->AverageLoc() - init_loc.x);
    disp_x += tmp_disp_x;

    // compute average discrepancy to the average location
    int32_t sz = static_cast<int32_t>(aux_ptr->SubLocs().size());
    double tmp_discrepancy = 0;
    for (auto &loc_x : aux_ptr->SubLocs()) {
      tmp_discrepancy += std::fabs(loc_x - aux_ptr->AverageLoc());
    }
    tmp_discrepancy = tmp_discrepancy / sz;
    discrepancy += tmp_discrepancy;
    max_discrepancy_ = std::max(max_discrepancy_, tmp_discrepancy);
  }

  displacements_.push_back(disp_x);
  discrepancies_.push_back(discrepancy);

  BOOST_LOG_TRIVIAL(info)
    << "Iter " << i << ", displacement: " << disp_x
    << ", discrepancy: " << discrepancy << "\n";
}

bool Stripe::IsDiscrepancyConverge() {
  if (discrepancies_.size() <= 1) return false;
  size_t sz = discrepancies_.size();
  double last_difference =
      std::fabs(discrepancies_[sz - 2] - discrepancies_[sz - 1]);
  double threshold = 0.001;
  return last_difference / discrepancies_[0] < threshold;
}

void Stripe::SetBlockLoc() {
  size_t sz = blk_ptrs_vec_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    Block *blk_ptr = blk_ptrs_vec_[i];
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    blk_ptr->SetLLX(std::round(aux_ptr->AverageLoc()));
  }
}

void Stripe::ClearMultiRowCellBreaking() {
  row_seg_ptrs_.clear();
}

void Stripe::IterativeCellReordering(int32_t max_iter, int32_t number_of_threads) {
  CollectAllRowSegments();
  omp_set_num_threads(number_of_threads);
  bool is_weighted_anchor = false;
  for (int32_t i = 0; i < max_iter; ++i) {
    //double decay = 30.0; // the bigger, the closer to CPLEX result
    //double lambda = exp(-i / decay);
    double lambda = 1 / double(i + 1);
    OptimizeDisplacementInEachRowSegment(
        lambda, is_weighted_anchor, i % 10 == 0
    );
    ComputeAverageLoc();
    ReportIterativeStatus(i);
    if (!is_weighted_anchor) {
      is_weighted_anchor = IsDiscrepancyConverge();
    }
    if (max_discrepancy_ < 0.1) break;
  }
  SetBlockLoc();
  omp_set_num_threads(1);
  ClearMultiRowCellBreaking();
  BOOST_LOG_TRIVIAL(info) << "displacement: " << displacements_ << "\n";
  BOOST_LOG_TRIVIAL(info) << "discrepancy : " << discrepancies_ << "\n";
}

void Stripe::SortBlocksInEachRow() {
  for (auto &row : gridded_rows_) {
    row.SortBlockRegions();
  }
}

size_t Stripe::OutOfBoundCell() {
  size_t cnt = 0;
  for (auto &row : gridded_rows_) {
    cnt += row.OutOfBoundCell();
  }
  return cnt;
}

#if DALI_USE_CPLEX
void Stripe::PopulateVariableArray(IloModel &model, IloNumVarArray &x) {
  IloEnv env = model.getEnv();
  IloInt cnt = 0;
  for (auto &row: gridded_rows_) {
    for (auto &blk_region: row.blk_regions_) {
      Block *blk_ptr = blk_region.p_blk;
      if (blk_ptr_2_tmp_id.find(blk_ptr) == blk_ptr_2_tmp_id.end()) {
        //x.add(IloNumVar(env, lx_, lx_ + width_));
        //x.add(IloNumVar(env, lx_, IloInfinity));
        x.add(IloNumVar(env, -IloInfinity, IloInfinity));
        blk_ptr_2_tmp_id[blk_ptr] = cnt;
        blk_tmp_id_2_ptr[cnt] = blk_ptr;
        ++cnt;
      }
    }
  }
}

void Stripe::AddVariableConstraints(
    IloModel &model,
    IloNumVarArray &x,
    IloRangeArray &c
) {
  for (auto &row: gridded_rows_) {
    size_t blk_cnt = row.blk_regions_.size();
    for (size_t i = 0; i < blk_cnt; ++i) {
      if (i > 0) {
        Block *blk_ptr0 = row.blk_regions_[i - 1].p_blk;
        IloInt id0 = blk_ptr_2_tmp_id[blk_ptr0];
        int32_t width = blk_ptr0->Width();
        Block *blk_ptr1 = row.blk_regions_[i].p_blk;
        IloInt id1 = blk_ptr_2_tmp_id[blk_ptr1];
        c.add(x[id1] - x[id0] >= width);
      }
    }
  }

  model.add(c);
}

void Stripe::ConstructQuadraticObjective(
    IloModel &model,
    IloNumVarArray &x
) {
  IloEnv env = model.getEnv();
  IloExpr objExpr(env);
  for (auto &row: gridded_rows_) {
    for (auto &blk_region: row.blk_regions_) {
      Block *blk_ptr = blk_region.p_blk;
      auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
      double2d init = aux_ptr->InitLoc();
      IloInt id = blk_ptr_2_tmp_id[blk_ptr];
      objExpr += 1.0 * x[id] * x[id] - 2 * init.x * x[id];
    }
  }
  IloObjective obj = IloMinimize(env, objExpr);
  model.add(obj);
  objExpr.end();
}

void Stripe::CreateQPModel(
    IloModel &model,
    IloNumVarArray &x,
    IloRangeArray &c
) {
  PopulateVariableArray(model, x);
  AddVariableConstraints(model, x, c);
  ConstructQuadraticObjective(model, x);
}

bool Stripe::SolveQPProblem(IloCplex &cplex, IloNumVarArray &var) {
  IloEnv env = var.getEnv();

  IloBool is_solved = cplex.solve();

  if (is_solved) {
    //BOOST_LOG_TRIVIAL(info)
    //  << "Solution status = " << cplex.getStatus() << "\n";
    //BOOST_LOG_TRIVIAL(info)
    //  << "Solution value  = " << cplex.getObjValue() << "\n";

    IloNumArray val(env);
    cplex.getValues(val, var);
    IloInt nvars = var.getSize();
    for (IloInt j = 0; j < nvars; ++j) {
      //env.out() << "Variable " << j << ": Value = " << val[j] << endl;
      Block *blk_ptr = blk_tmp_id_2_ptr[j];
      blk_ptr->SetLLX(val[j]);
    }
    val.end();
  } else {
    BOOST_LOG_TRIVIAL(info) << "Problem cannot be solved\n";
  }

  return is_solved;
}

bool Stripe::OptimizeDisplacementUsingQuadraticProgramming(int32_t number_of_threads) {
  bool is_solved = true;

  SortBlocksInEachRow();

  IloEnv env;
  try {
    // create a QP problem
    IloModel model(env);
    IloNumVarArray var(env);
    IloRangeArray con(env);
    CreateQPModel(model, var, con);

    IloCplex cplex(model);

    cplex.setParam(IloCplex::Param::MIP::Display, 0);
    cplex.setParam(IloCplex::Param::Threads, number_of_threads);

    // solve the QP problem
    is_solved = SolveQPProblem(cplex, var);
  } catch (IloException &e) {
    BOOST_LOG_TRIVIAL(error) << "Concert exception caught: " << e << "\n";
    is_solved = false;
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "Unknown exception caught" << "\n";
    is_solved = false;
  }

  env.end();
  return is_solved;
}
#endif

void Stripe::ImportStandardRowSegments(phydb::PhyDB &phydb, Circuit &ckt) {
  lx_ = INT_MAX;
  int32_t ux = INT_MIN;
  ly_ = INT_MAX;
  int32_t uy = INT_MIN;
  row_height_ = phydb.tech().GetSitesRef()[0].GetHeight() / ckt.GridValueY();
  auto &design = phydb.design();
  gridded_rows_.reserve(design.GetRowVec().size());
  for (auto &row : design.GetRowVec()) {
    gridded_rows_.emplace_back();
    auto &gridded_row = gridded_rows_.back();

    double d_llx = ckt.LocPhydb2DaliX(row.GetOriginX());
    DaliExpects(AbsResidual(d_llx, 1) < 1e-5, "row llx loc is not an integer");
    int32_t llx = static_cast<int32_t>(d_llx);
    gridded_row.SetLLX(llx);
    lx_ = std::min(lx_, gridded_row.LLX());

    double d_width = static_cast<double>(row.GetNumX()) * row.GetStepX()
        / ckt.DatabaseMicrons() / ckt.GridValueX();
    DaliExpects(AbsResidual(d_width, 1) < 1e-5,
                "row width loc is not an integer");
    int32_t width = static_cast<int32_t>(d_width);
    gridded_row.SetWidth(width);
    ux = std::max(ux, gridded_row.URX());

    double d_lly = ckt.LocPhydb2DaliY(row.GetOriginY());
    DaliExpects(AbsResidual(d_lly, 1) < 1e-5, "row lly loc is not an integer");
    int32_t lly = static_cast<int32_t>(d_lly);
    gridded_row.SetLLY(lly);
    gridded_row.SetHeight(row_height_);
    ly_ = std::min(ly_, gridded_row.LLY());
    uy = std::max(uy, gridded_row.URY());

    gridded_row.SetOrient(row.GetOrient() == phydb::CompOrient::N);

    std::vector<SegI> blockage;
    gridded_row.UpdateSegments(blockage, false);
  }
  width_ = ux - lx_;
  height_ = uy - ly_;
}

int32_t Stripe::LocY2RowId(double lly) {
  if (lly <= LLY() + row_height_ / 2.0) {
    return 0;
  }
  if (lly >= URY() - row_height_ / 2.0) {
    return static_cast<int32_t>(gridded_rows_.size()) - 1;
  }

  double height = lly - LLY();
  return static_cast<int32_t>(std::round(height / row_height_));
}

double Stripe::EstimateCost(
    int32_t row_id,
    Block *blk_ptr,
    SegI &range,
    double density
) {
  int32_t region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
  std::vector<SegI> spaces;
  spaces.emplace_back(LLX(), URX());
  for (int32_t i = 0; i < region_cnt; ++i) {
    gridded_rows_[row_id + i].UpdateCommonSegment(
        spaces,
        blk_ptr->Width(),
        density
    );
  }

  if (spaces.empty()) {
    return DBL_MAX;
  }
  int32_t sz = static_cast<int32_t>(spaces.size());
  int32_t min_id = -1;
  double min_cost = DBL_MAX;
  for (int32_t i = 0; i < sz; ++i) {
    SegI &space = spaces[i];
    double tmp_cost = DBL_MAX;
    if (space.lo <= blk_ptr->LLX() && space.hi >= blk_ptr->URX()) {
      tmp_cost = 0;
    }
    if (space.lo > blk_ptr->LLX()) {
      tmp_cost = space.lo - blk_ptr->LLX();
    }
    if (space.hi < blk_ptr->URX()) {
      tmp_cost = blk_ptr->URX() - space.hi;
    }
    if (tmp_cost < min_cost) {
      min_cost = tmp_cost;
      min_id = i;
    }
  }

  range = spaces[min_id];
  //return min_cost;
  double y_cost = std::fabs(blk_ptr->LLY() - gridded_rows_[row_id].LLY());
  return min_cost + y_cost;
}

void Stripe::AddBlockToRow(int32_t row_id, Block *blk_ptr, SegI range) {
  int32_t region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
  blk_ptr->SetLLY(gridded_rows_[row_id].LLY());
  for (int32_t i = 0; i < region_cnt; ++i) {
    gridded_rows_[row_id + i].AddStandardCell(blk_ptr, i, range);
  }
}

void Stripe::AssignStandardCellsToRowSegments(/*double white_space_usage*/) {
  std::sort(
      blk_ptrs_vec_.begin(),
      blk_ptrs_vec_.end(),
      [](const Block *blk0, const Block *blk1) {
        return (blk0->LLY() < blk1->LLY()) ||
            ((blk0->LLY() == blk1->LLY()) && (blk0->Id() < blk1->Id()));
      }
  );
  //int32_t row_cnt = static_cast<int32_t>(gridded_rows_.size());
  for (auto &blk_ptr : blk_ptrs_vec_) {
    int32_t row_id = LocY2RowId(blk_ptr->LLY());
    SegI range(blk_ptr->LLX(), blk_ptr->URX());
    AddBlockToRow(row_id, blk_ptr, range);
  }
}

Stripe *ClusterStripe::GetStripeMatchSeg(SegI seg, int32_t y_loc) {
  Stripe *res = nullptr;
  for (auto &Stripe : stripe_list_) {
    if ((Stripe.URY() == y_loc) && (Stripe.LLX() == seg.lo)
        && (Stripe.URX() == seg.hi)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeMatchBlk(Block *blk_ptr) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  for (auto &&Stripe : stripe_list_) {
    if ((Stripe.LLY() <= center_y) &&
        (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) &&
        (Stripe.URX() > center_x)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeClosestToBlk(Block *blk_ptr, double &distance) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto &Stripe : stripe_list_) {
    double tmp_distance;
    if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      res = &Stripe;
      tmp_distance = 0;
    } else if ((Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      tmp_distance = std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    } else if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y)) {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      );
    } else {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      ) + std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &Stripe;
    }
  }

  distance = min_distance;
  return res;
}

void ClusterStripe::AssignBlockToSimpleStripe() {
  for (auto &Stripe : stripe_list_) {
    Stripe.block_count_ = 0;
    Stripe.blk_ptrs_vec_.clear();
  }

  for (auto &blk_ptr : block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_count_++;
  }

  for (auto &Stripe : stripe_list_) {
    Stripe.blk_ptrs_vec_.reserve(Stripe.block_count_);
  }

  for (auto &blk_ptr : block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->blk_ptrs_vec_.push_back(blk_ptr);
  }
}

}