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

/**
 * @file
 * Stripe and StripeColumn: the regions gridded legalization works within.
 *
 * A stripe is the unit legalization succeeds or fails on. Rows are grown into
 * it from a contour that advances as clusters are added, so a stripe whose used
 * height exceeds its available height has failed and the flow reports it rather
 * than emitting an illegal placement.
 *
 * A stripe column is a full-height slice of the region and its own well region;
 * see [architecture notes](../ARCHITECTURE.md) for how the two relate to rows
 * and segments.
 */
#include "stripe.h"

#include <omp.h>

#include <algorithm>
#include <cfloat>
#include <climits>

#include "dali/placer/well_legalizer/component_helper.h"
#include "dali/placer/well_legalizer/component_legalization_state.h"
#include "dali/placer/well_legalizer/stripe_helper.h"

namespace dali {

bool Stripe::HasNoRowsSpillingOut() const {
  if (gridded_rows_.empty()) return true;

  bool is_first_row_fully_in =
      gridded_rows_[0].LLY() >= LLY() && gridded_rows_[0].URY() <= URY();
  bool is_last_row_fully_in = gridded_rows_.back().LLY() >= LLY() &&
                              gridded_rows_.back().URY() <= URY();

  return is_first_row_fully_in && is_last_row_fully_in;
}

/** Nudge each row's cells to minimum displacement from their start locations. */
void Stripe::MinDisplacementAdjustment() {
  for (auto& row : gridded_rows_) {
    row.UpdateMinDisplacementLLY();
  }

  std::sort(gridded_rows_.begin(), gridded_rows_.end(),
            [](const GriddedRow& cluster0, const GriddedRow& cluster1) {
              return cluster0.MinDisplacementLLY() <
                     cluster1.MinDisplacementLLY();
            });

  std::vector<VerticalRowSegment> segments;

  int sz = (int)gridded_rows_.size();
  int lower_bound = ly_;
  int upper_bound = ly_ + height_;
  for (int i = 0; i < sz; ++i) {
    // create a segment which contains only this component
    GriddedRow& row = gridded_rows_[i];
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

    // two segments repeats until this is no overlap or only one segment left

    VerticalRowSegment* cur_seg = &(segments[seg_sz - 1]);
    VerticalRowSegment* prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->OverlapsNextRowSegment(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  for (auto& seg : segments) {
    seg.UpdateRowLocations();
  }
}

void Stripe::SortComponentsBasedOnLLY() {
  std::sort(component_ptrs_vec_.begin(), component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
}

void Stripe::SortComponentsBasedOnURY() {
  std::sort(component_ptrs_vec_.begin(), component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() < rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
}

void Stripe::SortComponentsBasedOnStretchedURY() {
  std::sort(component_ptrs_vec_.begin(), component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
}

void Stripe::SortComponentsBasedOnYLocation(int criterion) {
  switch (criterion) {
    case 0: {
      SortComponentsBasedOnLLY();
      break;
    }
    case 1: {
      SortComponentsBasedOnURY();
      break;
    }
    case 2: {
      SortComponentsBasedOnStretchedURY();
      break;
    }
    default: {
      DaliExpects(false, "unknown component sorting criterion");
    }
  }
}

/**
 * Work out where taps will go in each row before physical completion runs.
 * @param is_checker_board_mode stagger tap columns between adjacent rows.
 * @param tap_cell_interval_grid spacing between taps, in grid units.
 */
void Stripe::PrecomputeWellTapCellLocation(bool is_checker_board_mode,
                                           int tap_cell_interval_grid,
                                           Macro* well_tap_macro) {
  is_checkerboard_mode_ = is_checker_board_mode;
  DaliExpects(tap_cell_interval_grid > 0,
              "Non-positive well-tap cell interval?");
  DaliExpects(well_tap_macro != nullptr, "Well-tap cell is a nullptr?");
  well_tap_width_ = well_tap_macro->Width();
  DaliExpects(width_ > well_tap_width_,
              "Stripe width is smaller than well-tap cell width?");

  std::vector<int> locations;

  if (is_checkerboard_mode_) {
    if (tap_cell_interval_grid & 1) {  // if this interval is an odd number
      int new_tap_cell_interval_grid = tap_cell_interval_grid - 1;
      LOG(info) << "Rounding well tap cell interval from "
                << tap_cell_interval_grid << " to "
                << new_tap_cell_interval_grid << "\n";
      tap_cell_interval_grid = new_tap_cell_interval_grid;
    }
    tap_cell_interval_grid = tap_cell_interval_grid / 2;
  }

  int space_to_left = 0;  // TODO: this can be a parameter exposed to users
  int well_tap_cell_loc = lx_ + space_to_left;
  int number_of_well_tap_cell =
      (width_ - space_to_left) / tap_cell_interval_grid + 1;
  for (int i = 0; i < number_of_well_tap_cell; ++i) {
    locations.emplace_back(well_tap_cell_loc);
    well_tap_cell_loc += tap_cell_interval_grid;
  }

  int last_ux = locations.back() + well_tap_width_;
  int right_most_loc = URX() - well_tap_width_;
  if (last_ux > URX()) {
    locations.back() = right_most_loc;
  } else {
    // interval
    if (URX() - last_ux >= tap_cell_interval_grid / 2.0) {
      locations.emplace_back(right_most_loc);
    }
  }

  // finalize well-tap cell locations
  if (is_checkerboard_mode_) {
    size_t sz = locations.size();
    size_t half_sz = (sz + 1) >> 1;  // divide by 2
    well_tap_cell_location_even_.reserve(half_sz);
    well_tap_cell_location_odd_.reserve(half_sz);
    for (size_t i = 0; i < sz; ++i) {
      int lo_loc = locations[i];
      int hi_loc = locations[i] + well_tap_width_;
      if (i & 1) {  // if index i is an odd number
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
      int lo_loc = locations[i];
      int hi_loc = locations[i] + well_tap_width_;
      well_tap_cell_location_odd_.emplace_back(lo_loc, hi_loc);
      well_tap_cell_location_even_.emplace_back(lo_loc, hi_loc);
    }
  }
}

void Stripe::UpdateFrontClusterUpward(int p_height, int n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the lower y location of the front cluster
  int ly;
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
 * Add the corresponding component region into these clusters based on row
 * orientation and component orientation.
 *
 * @param component
 */
void Stripe::SimplyAddFollowingClusters(Component* component, bool is_upward) {
  int region_count = component->MacroPtr()->RegionCount();
  for (int i = 1; i < region_count; ++i) {
    int row_index = front_id_ + i;
    if (row_index >= static_cast<int>(gridded_rows_.size())) {
      gridded_rows_.emplace_back();
    }
    bool is_orient_N = !gridded_rows_[row_index - 1].IsOrientN();
    gridded_rows_[row_index].SetOrient(is_orient_N);
    int region_id = is_upward ? i : region_count - 1 - i;
    gridded_rows_[row_index].AddComponentRegion(component, region_id, true);
  }
}

bool Stripe::AddComponentToFrontCluster(Component* component, bool is_upward) {
  bool res = gridded_rows_[front_id_].AttemptToAdd(component, is_upward);
  if (!res) return false;

  // add this component to other clusters above the front cluster
  SimplyAddFollowingClusters(component, is_upward);

  return true;
}

bool Stripe::AddComponentToFrontClusterWithDispCheck(
    Component* component, double displacement_upper_limit, bool is_upward) {
  bool res = gridded_rows_[front_id_].AttemptToAddWithDispCheck(
      component, displacement_upper_limit, is_upward);
  if (!res) return false;

  // add this component to other clusters above the front cluster
  SimplyAddFollowingClusters(component, is_upward);

  return true;
}

size_t Stripe::FitComponentsToFrontSpaceUpward(size_t start_id,
                                               int current_iteration) {
  std::vector<Component*> legalized_components;
  std::vector<Component*> skipped_components;

  size_t components_sz = component_ptrs_vec_.size();
  for (size_t i = start_id; i < components_sz; ++i) {
    Component* component = component_ptrs_vec_[i];
    if (!gridded_rows_[front_id_].IsOverlap(component, current_iteration,
                                            true)) {
      break;
    }
    if (gridded_rows_[front_id_].IsOrientMatching(component, 0)) {
      if (AddComponentToFrontCluster(component, true)) {
        legalized_components.push_back(component);
      } else {
        skipped_components.push_back(component);
      }
    } else {
      skipped_components.push_back(component);
    }
  }

  // put legalized components back to the sorted list
  for (size_t i = 0; i < legalized_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = legalized_components[i];
  }

  // put skipped components back to the sorted list
  start_id = start_id + legalized_components.size();
  for (size_t i = 0; i < skipped_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = skipped_components[i];
  }

  return start_id;
}

size_t Stripe::FitComponentsToFrontSpaceUpwardWithDispCheck(
    size_t start_id, double displacement_upper_limit) {
  std::vector<Component*> legalized_components;
  std::vector<Component*> skipped_components;

  size_t components_sz = component_ptrs_vec_.size();
  for (size_t i = start_id; i < components_sz; ++i) {
    Component* component = component_ptrs_vec_[i];
    if (gridded_rows_[front_id_].IsOrientMatching(component, 0)) {
      if (AddComponentToFrontClusterWithDispCheck(
              component, displacement_upper_limit, true)) {
        legalized_components.push_back(component);
      } else {
        skipped_components.push_back(component);
      }
    } else {
      skipped_components.push_back(component);
    }
  }

  // put legalized components back to the sorted list
  for (size_t i = 0; i < legalized_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = legalized_components[i];
  }

  // put skipped components back to the sorted list
  start_id = start_id + legalized_components.size();
  for (size_t i = 0; i < skipped_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = skipped_components[i];
  }

  return start_id;
}

void Stripe::LegalizeFrontCluster(bool use_init_loc) {
  gridded_rows_[front_id_].LegalizeSegmentsX(use_init_loc);
}

void Stripe::UpdateRemainingClusters(int p_height, int n_height,
                                     bool is_upward) {
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

void Stripe::UpdateComponentStretchLength() {
  if (!is_bottom_up_) {
    std::reverse(gridded_rows_.begin(), gridded_rows_.end());
    is_bottom_up_ = true;
  }

  for (auto& row : gridded_rows_) {
    row.InitializeComponentStretching();
  }

  int sz = static_cast<int>(gridded_rows_.size());
  for (int i = 1; i < sz; ++i) {
    GriddedRow& cur_cluster = gridded_rows_[i];
    GriddedRow& pre_cluster = gridded_rows_[i - 1];
    for (auto& component_region : cur_cluster.component_regions_) {
      int id = component_region.region_id;
      if (id >= 1) {
        Component* component = component_region.component;
        --id;
        int well_edge_distance =
            component_region.component->MacroPtr()->AdjacentRegionEdgeDistance(
                id, component->IsFlipped());
        int actual_edge_distance = (cur_cluster.LLY() + cur_cluster.PNEdge()) -
                                   (pre_cluster.LLY() + pre_cluster.PNEdge());
        int length = actual_edge_distance - well_edge_distance;
        component->SetStretchLength(id, length);
      }
    }
  }
}

void Stripe::UpdateFrontClusterDownward(int p_height, int n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the upper y of the front cluster
  int uy;
  bool is_orient_N;
  if (front_id_ == 0) {
    uy = URY();
    if (component_ptrs_vec_.empty()) {
      is_orient_N = is_first_row_orient_N_;
    } else {
      Macro* macro_ptr = component_ptrs_vec_[0]->MacroPtr();
      bool is_component_flipped = component_ptrs_vec_[0]->IsFlipped();
      if (is_component_flipped) {
        is_orient_N = !macro_ptr->IsNwellAbovePwell(0);
      } else {
        int region_id = macro_ptr->RegionCount() - 1;
        is_orient_N = macro_ptr->IsNwellAbovePwell(region_id);
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

size_t Stripe::FitComponentsToFrontSpaceDownward(size_t start_id,
                                                 int current_iteration) {
  std::vector<Component*> legalized_components;
  std::vector<Component*> skipped_components;

  size_t components_sz = component_ptrs_vec_.size();
  for (size_t i = start_id; i < components_sz; ++i) {
    Component* component = component_ptrs_vec_[i];
    if (!gridded_rows_[front_id_].IsOverlap(component, current_iteration,
                                            false)) {
      break;
    }
    int region_id = component->MacroPtr()->RegionCount() - 1;
    if (gridded_rows_[front_id_].IsOrientMatching(component, region_id)) {
      if (AddComponentToFrontCluster(component, false)) {
        legalized_components.push_back(component);
      } else {
        skipped_components.push_back(component);
      }
    } else {
      skipped_components.push_back(component);
    }
  }

  // put legalized components back to the sorted list
  for (size_t i = 0; i < legalized_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = legalized_components[i];
  }

  // put skipped components back to the sorted list
  start_id = start_id + legalized_components.size();
  for (size_t i = 0; i < skipped_components.size(); ++i) {
    component_ptrs_vec_[i + start_id] = skipped_components[i];
  }

  return start_id;
}

/****
 * @brief Update the Y location of all cells
 */
void Stripe::UpdateComponentYLocation() {
  for (GriddedRow& row : gridded_rows_) {
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
 * real row segments, and thus they need to be cleaned up in order not to
 * confuse following processes.
 *
 */
void Stripe::CleanUpTemporaryRowSegments() {
  int row_cnt = static_cast<int>(gridded_rows_.size());
  for (int i = 0; i < row_cnt; ++i) {
    GriddedRow& row = gridded_rows_[i];
    // remove all temporary and intrinsic segments
    row.Segments().clear();
    // re-create intrinsic segments
    if (i & 1) {
      row.UpdateSegments(well_tap_cell_location_odd_, false);
    } else {
      row.UpdateSegments(well_tap_cell_location_even_, false);
    }
    // assign components back to row segments
    row.AssignComponentsToSegments();
  }
}

size_t Stripe::AddWellTapCells(Circuit* p_ckt, Macro* well_tap_macro,
                               size_t start_id) {
  size_t row_cnt = gridded_rows_.size();
  for (size_t i = 0; i < row_cnt; ++i) {
    if (i & 1) {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_macro, start_id, well_tap_cell_location_odd_);
    } else {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_macro, start_id, well_tap_cell_location_even_);
    }
  }
  return start_id;
}

bool Stripe::IsLeftmostPlacementLegal() {
  std::sort(
      component_ptrs_vec_.begin(), component_ptrs_vec_.end(),
      [](const Component* blk0, const Component* blk1) {
        return (blk0->LLX() < blk1->LLX()) ||
               ((blk0->LLX() == blk1->LLX()) && (blk0->Id() < blk1->Id()));
      });

  DaliExpects(false, "To be implemented");

  return true;
}

bool Stripe::IsStripeLegal() {
  for (GriddedRow& row : gridded_rows_) {
    if (!row.IsRowLegal()) return false;
  }
  return true;
}

void Stripe::CollectAllRowSegments() {
  size_t row_seg_cnt = 0;
  for (GriddedRow& row : gridded_rows_) {
    row_seg_cnt += row.segments_.size();
  }
  row_seg_ptrs_.reserve(row_seg_cnt);
  for (GriddedRow& row : gridded_rows_) {
    for (RowSegment& segment : row.segments_) {
      row_seg_ptrs_.push_back(&segment);
    }
  }
}

void Stripe::UpdateSubCellLocs(
    std::vector<ComponentDisplacementVariable>& vars) {
  for (ComponentDisplacementVariable& var : vars) {
    Component* component_ptr = var.component_region.component;
    if (component_ptr == nullptr) continue;  // skip dummy cells
    auto* aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    aux_ptr->SetSubCellLoc(var.component_region.region_id, var.Solution(),
                           var.SegmentWeight());
  }
}

void Stripe::OptimizeDisplacementInEachRowSegment(double lambda,
                                                  bool is_weighted_anchor,
                                                  bool is_reorder) {
  size_t sz = row_seg_ptrs_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    RowSegment* seg = row_seg_ptrs_[i];
    std::vector<ComponentDisplacementVariable> vars =
        seg->OptimizeQuadraticDisplacement(lambda, is_weighted_anchor,
                                           is_reorder);
    // std::vector<ComponentDisplacementVariable> vars =
    //     is_reorder);
    UpdateSubCellLocs(vars);
  }
}

void Stripe::ComputeAverageLoc() {
  size_t sz = component_ptrs_vec_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    Component* component_ptr = component_ptrs_vec_[i];
    auto aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    aux_ptr->ComputeAverageLoc();
    component_ptr->SetLLX(aux_ptr->AverageLoc());
  }
}

void Stripe::ReportIterativeStatus(int i) {
  double disp_x = 0;
  double discrepancy = 0;
  max_discrepancy_ = 0;
  for (auto& component_ptr : component_ptrs_vec_) {
    auto aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(aux_ptr->AverageLoc() - init_loc.x);
    disp_x += tmp_disp_x;

    int sz = static_cast<int>(aux_ptr->SubLocs().size());
    double tmp_discrepancy = 0;
    for (auto& loc_x : aux_ptr->SubLocs()) {
      tmp_discrepancy += std::fabs(loc_x - aux_ptr->AverageLoc());
    }
    tmp_discrepancy = tmp_discrepancy / sz;
    discrepancy += tmp_discrepancy;
    max_discrepancy_ = std::max(max_discrepancy_, tmp_discrepancy);
  }

  displacements_.push_back(disp_x);
  discrepancies_.push_back(discrepancy);

  LOG(info) << "Iter " << i << ", displacement: " << disp_x
            << ", discrepancy: " << discrepancy << "\n";
}

bool Stripe::IsDiscrepancyConverged() {
  if (discrepancies_.size() <= 1) return false;
  size_t sz = discrepancies_.size();
  double last_difference =
      std::fabs(discrepancies_[sz - 2] - discrepancies_[sz - 1]);
  double threshold = 0.001;
  return last_difference / discrepancies_[0] < threshold;
}

void Stripe::SetComponentLoc() {
  size_t sz = component_ptrs_vec_.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    Component* component_ptr = component_ptrs_vec_[i];
    auto aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    component_ptr->SetLLX(std::round(aux_ptr->AverageLoc()));
  }
}

void Stripe::ClearMultiRowCellBreaking() { row_seg_ptrs_.clear(); }

void Stripe::IterativeCellReordering(int max_iter, int number_of_threads) {
  CollectAllRowSegments();
  omp_set_num_threads(number_of_threads);
  bool is_weighted_anchor = false;
  for (int i = 0; i < max_iter; ++i) {
    // double decay = 30.0;
    // double lambda = exp(-i / decay);
    double lambda = 1 / double(i + 1);
    OptimizeDisplacementInEachRowSegment(lambda, is_weighted_anchor,
                                         i % 10 == 0);
    ComputeAverageLoc();
    ReportIterativeStatus(i);
    if (!is_weighted_anchor) {
      is_weighted_anchor = IsDiscrepancyConverged();
    }
    if (max_discrepancy_ < 0.1) break;
  }
  SetComponentLoc();
  omp_set_num_threads(1);
  ClearMultiRowCellBreaking();
  LOG(info) << "displacement: " << displacements_ << "\n";
  LOG(info) << "discrepancy : " << discrepancies_ << "\n";
}

void Stripe::SortComponentsInEachRow() {
  for (auto& row : gridded_rows_) {
    row.SortComponentRegions();
  }
}

size_t Stripe::OutOfBoundCell() {
  size_t cnt = 0;
  for (auto& row : gridded_rows_) {
    cnt += row.OutOfBoundCell();
  }
  return cnt;
}

/**
 * Populate this stripe's rows from PhyDB's standard-cell row definitions.
 *
 * The path for a design already placed on standard-cell rows, as opposed to one
 * clustered into gridded rows here.
 */
void Stripe::ImportStandardRowSegments(phydb::PhyDB& phydb, Circuit& ckt) {
  lx_ = INT_MAX;
  int ux = INT_MIN;
  ly_ = INT_MAX;
  int uy = INT_MIN;
  row_height_ = phydb.tech().GetSitesRef()[0].GetHeight() / ckt.GridValueY();
  auto& design = phydb.design();
  gridded_rows_.reserve(design.GetRowVec().size());
  for (auto& row : design.GetRowVec()) {
    gridded_rows_.emplace_back();
    auto& gridded_row = gridded_rows_.back();

    double d_llx = ckt.LocPhydb2DaliX(row.GetOriginX());
    DaliExpects(AbsResidual(d_llx, 1) < 1e-5, "row llx loc is not an integer");
    int llx = static_cast<int>(d_llx);
    gridded_row.SetLLX(llx);
    lx_ = std::min(lx_, gridded_row.LLX());

    double d_width = static_cast<double>(row.GetNumX()) * row.GetStepX() /
                     ckt.DatabaseMicrons() / ckt.GridValueX();
    DaliExpects(AbsResidual(d_width, 1) < 1e-5,
                "row width loc is not an integer");
    int width = static_cast<int>(d_width);
    gridded_row.SetWidth(width);
    ux = std::max(ux, gridded_row.URX());

    double d_lly = ckt.LocPhydb2DaliY(row.GetOriginY());
    DaliExpects(AbsResidual(d_lly, 1) < 1e-5, "row lly loc is not an integer");
    int lly = static_cast<int>(d_lly);
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

int Stripe::LocY2RowId(double lly) {
  if (lly <= LLY() + row_height_ / 2.0) {
    return 0;
  }
  if (lly >= URY() - row_height_ / 2.0) {
    return static_cast<int>(gridded_rows_.size()) - 1;
  }

  double height = lly - LLY();
  return static_cast<int>(std::round(height / row_height_));
}

double Stripe::EstimateCost(int row_id, Component* component_ptr, SegI& range,
                            double density) {
  int region_cnt = component_ptr->MacroPtr()->RegionCount();
  std::vector<SegI> spaces;
  spaces.emplace_back(LLX(), URX());
  for (int i = 0; i < region_cnt; ++i) {
    gridded_rows_[row_id + i].UpdateCommonSegment(
        spaces, component_ptr->Width(), density);
  }

  if (spaces.empty()) {
    return DBL_MAX;
  }
  int sz = static_cast<int>(spaces.size());
  int min_id = -1;
  double min_cost = DBL_MAX;
  for (int i = 0; i < sz; ++i) {
    SegI& space = spaces[i];
    double tmp_cost = DBL_MAX;
    if (space.lo <= component_ptr->LLX() && space.hi >= component_ptr->URX()) {
      tmp_cost = 0;
    }
    if (space.lo > component_ptr->LLX()) {
      tmp_cost = space.lo - component_ptr->LLX();
    }
    if (space.hi < component_ptr->URX()) {
      tmp_cost = component_ptr->URX() - space.hi;
    }
    if (tmp_cost < min_cost) {
      min_cost = tmp_cost;
      min_id = i;
    }
  }

  range = spaces[min_id];
  double y_cost = std::fabs(component_ptr->LLY() - gridded_rows_[row_id].LLY());
  return min_cost + y_cost;
}

void Stripe::AddComponentToRow(int row_id, Component* component_ptr,
                               SegI range) {
  int region_cnt = component_ptr->MacroPtr()->RegionCount();
  component_ptr->SetLLY(gridded_rows_[row_id].LLY());
  for (int i = 0; i < region_cnt; ++i) {
    gridded_rows_[row_id + i].AddStandardCell(component_ptr, i, range);
  }
}

void Stripe::AssignStandardCellsToRowSegments(/*double white_space_usage*/) {
  std::sort(
      component_ptrs_vec_.begin(), component_ptrs_vec_.end(),
      [](const Component* blk0, const Component* blk1) {
        return (blk0->LLY() < blk1->LLY()) ||
               ((blk0->LLY() == blk1->LLY()) && (blk0->Id() < blk1->Id()));
      });
  // int row_cnt = static_cast<int>(gridded_rows_.size());
  for (auto& component_ptr : component_ptrs_vec_) {
    int row_id = LocY2RowId(component_ptr->LLY());
    SegI range(component_ptr->LLX(), component_ptr->URX());
    AddComponentToRow(row_id, component_ptr, range);
  }
}

Stripe* StripeColumn::GetStripeMatchSeg(SegI seg, int y_loc) {
  Stripe* res = nullptr;
  for (auto& Stripe : stripe_list_) {
    if ((Stripe.URY() == y_loc) && (Stripe.LLX() == seg.lo) &&
        (Stripe.URX() == seg.hi)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe* StripeColumn::GetStripeMatchComponent(Component* component_ptr) {
  Stripe* res = nullptr;
  double center_x = component_ptr->X();
  double center_y = component_ptr->Y();
  for (auto& Stripe : stripe_list_) {
    if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe* StripeColumn::GetStripeClosestToComponent(Component* component_ptr,
                                                  double& distance) {
  Stripe* res = nullptr;
  double center_x = component_ptr->X();
  double center_y = component_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto& Stripe : stripe_list_) {
    double tmp_distance;
    if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      res = &Stripe;
      tmp_distance = 0;
    } else if ((Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      tmp_distance = std::min(std::abs(center_y - Stripe.LLY()),
                              std::abs(center_y - Stripe.URY()));
    } else if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y)) {
      tmp_distance = std::min(std::abs(center_x - Stripe.LLX()),
                              std::abs(center_x - Stripe.URX()));
    } else {
      tmp_distance = std::min(std::abs(center_x - Stripe.LLX()),
                              std::abs(center_x - Stripe.URX())) +
                     std::min(std::abs(center_y - Stripe.LLY()),
                              std::abs(center_y - Stripe.URY()));
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &Stripe;
    }
  }

  distance = min_distance;
  return res;
}

void StripeColumn::AssignComponentToSimpleStripe() {
  for (auto& Stripe : stripe_list_) {
    Stripe.component_count_ = 0;
    Stripe.component_ptrs_vec_.clear();
  }

  for (auto& component_ptr : component_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToComponent(component_ptr, tmp_dist);
    Stripe->component_count_++;
  }

  for (auto& Stripe : stripe_list_) {
    Stripe.component_ptrs_vec_.reserve(Stripe.component_count_);
  }

  for (auto& component_ptr : component_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToComponent(component_ptr, tmp_dist);
    Stripe->component_ptrs_vec_.push_back(component_ptr);
  }
}

}  // namespace dali
