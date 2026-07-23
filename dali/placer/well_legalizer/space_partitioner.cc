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
 * Divides the placement region into stripe columns before well legalization.
 *
 * Column width is driven by MaxPlugDist: every transistor must sit within that
 * distance of a compatible-well tap, which bounds how wide a row may be.
 * Neighbouring columns are then separated by `well_spacing`, so each column
 * becomes an independent well region. `StartPartitioning` fetches the well
 * parameters, chooses column boundaries, assigns components to columns by
 * available whitespace, and finally cuts each column into stripes.
 *
 * In scavenge mode the last column absorbs whatever space is left over rather
 * than stopping at its nominal boundary; strict mode leaves that space unused.
 */
#include "space_partitioner.h"

#include <algorithm>
#include <cfloat>
#include <map>

#include "dali/placer/well_legalizer/packed_stripe_boundary_planner.h"

namespace dali {

void SpacePartitioner::SetCircuit(Circuit* circuit) {
  DaliExpects(circuit != nullptr, "Partition space for a null Circuit?");
  circuit_ = circuit;
}

void SpacePartitioner::SetOutput(std::vector<StripeColumn>* output_stripes) {
  DaliExpects(output_stripes != nullptr,
              "Save partitioning result to a nullptr?");
  output_stripes_ = output_stripes;
}

void SpacePartitioner::SetReservedSpaceToBoundaries(int l_space, int r_space,
                                                    int b_space, int t_space) {
  l_space_ = l_space;
  r_space_ = r_space;
  b_space_ = b_space;
  t_space_ = t_space;
}

void SpacePartitioner::SetPartitionMode(int partition_mode) {
  partition_mode_ = partition_mode;
}

void SpacePartitioner::SetMaxRowWidth(int max_row_width) {
  max_row_width_ = max_row_width;
}

void SpacePartitioner::SetAdaptiveStripeBoundaries(
    bool enable, const GriddedCapacityConfig& capacity_config) {
  use_adaptive_boundaries_ = enable;
  capacity_config_ = capacity_config;
}

void SpacePartitioner::SetAdaptiveBoundaryBlend(double blend) {
  DaliExpects(blend >= 0.0 && blend <= 1.0,
              "Adaptive stripe boundary blend must be in [0, 1]");
  adaptive_boundary_blend_ = blend;
}

void SpacePartitioner::SetColumnBoundaries(const std::vector<int>& boundaries) {
  column_boundaries_override_ = boundaries;
}

void WellSpacePartitioner::FetchWellParameters() {
  Tech& tech = circuit_->tech();
  WellLayer& n_well_layer = tech.NwellLayer();
  double grid_value_x = circuit_->GridValueX();
  int same_well_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_well_spacing =
      std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ =
      (int)std::floor(n_well_layer.MaxPlugDist() / grid_value_x);
}

/** Compute the placeable whitespace per row, before columns are cut. */
void WellSpacePartitioner::DetectAvailSpace() {
  if (!row_height_set_) {
    row_height_ = circuit_->RowHeightGridUnit();
  }
  tot_num_rows_ = (Top() - Bottom()) / row_height_;

  std::vector<std::vector<SegI>> macro_segments;
  macro_segments.resize(tot_num_rows_);
  SegI tmp(0, 0);
  bool out_of_range;
  for (auto& component : circuit_->Components()) {
    if (component.IsMovable()) continue;
    int ly = int(std::floor(component.LLY()));
    int uy = int(std::ceil(component.URY()));
    int lx = int(std::floor(component.LLX()));
    int ux = int(std::ceil(component.URX()));

    out_of_range =
        (ly >= Top()) || (uy <= Bottom()) || (lx >= Right()) || (ux <= Left());

    if (out_of_range) continue;

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    tmp.lo = std::max(Left(), lx);
    tmp.hi = std::min(Right(), ux);
    if (tmp.hi > tmp.lo) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto& intervals : macro_segments) {
    MergeIntervals(intervals);
  }

  std::vector<std::vector<int>> intermediate_seg_rows;
  intermediate_seg_rows.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    if (macro_segments[i].empty()) {
      intermediate_seg_rows[i].push_back(Left());
      intermediate_seg_rows[i].push_back(Right());
      continue;
    }
    int segments_size = int(macro_segments[i].size());
    for (int j = 0; j < segments_size; ++j) {
      auto& interval = macro_segments[i][j];
      if (interval.lo == Left() && interval.hi < Right()) {
        intermediate_seg_rows[i].push_back(interval.hi);
      }

      if (interval.lo > Left()) {
        if (intermediate_seg_rows[i].empty()) {
          intermediate_seg_rows[i].push_back(Left());
        }
        intermediate_seg_rows[i].push_back(interval.lo);
        if (interval.hi < Right()) {
          intermediate_seg_rows[i].push_back(interval.hi);
        }
      }
    }
    if (intermediate_seg_rows[i].size() % 2 == 1) {
      intermediate_seg_rows[i].push_back(Right());
    }
  }

  white_space_in_rows_.resize(tot_num_rows_);
  int min_component_width = int(circuit_->MinComponentWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    white_space_in_rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j] >=
          min_component_width) {
        white_space_in_rows_[i].emplace_back(intermediate_seg_rows[i][j],
                                             intermediate_seg_rows[i][j + 1]);
      }
    }
  }
}

void WellSpacePartitioner::UpdateWhiteSpaceInCol(StripeColumn& col) {
  SegI stripe_seg(col.LLX(), col.URX());
  col.white_space_.clear();
  col.white_space_.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    for (auto& seg : white_space_in_rows_[i]) {
      SegI* tmp_seg = stripe_seg.Joint(seg);
      if (tmp_seg != nullptr) {
        col.white_space_[i].push_back(*tmp_seg);
      }
      delete tmp_seg;
    }
  }
}

void WellSpacePartitioner::DecomposeSpaceToSimpleStripes() {
  for (auto& col : *output_stripes_) {
    for (int i = 0; i < tot_num_rows_; ++i) {
      for (auto& seg : col.white_space_[i]) {
        int y_loc = RowToLoc(i);
        Stripe* stripe = col.GetStripeMatchSeg(seg, y_loc);
        if (stripe == nullptr) {
          col.stripe_list_.emplace_back();
          stripe = &(col.stripe_list_.back());
          stripe->lx_ = seg.lo;
          stripe->width_ = seg.Span();
          stripe->ly_ = y_loc;
          stripe->height_ = row_height_;
          stripe->contour_ = y_loc;
          stripe->front_row_ = nullptr;
          stripe->used_height_ = 0;
          stripe->max_component_capacity_per_cluster_ =
              stripe->width_ / circuit_->MinComponentWidth();
        } else {
          stripe->height_ += row_height_;
        }
      }
    }
  }

  // col_list_[tot_col_num_ - 1].stripe_list_[0].width_ =
  // col_list_[tot_col_num_ -
  // 1].stripe_list_[0].max_component_capacity_per_cluster_ =
  //     col_list_[tot_col_num_ - 1].stripe_list_[0].width_ /
  }

/**
 * Assign each component to a column, balancing against available whitespace.
 */
void WellSpacePartitioner::AssignComponentToColBasedOnWhiteSpace() {
  std::vector<Component>& component_list = circuit_->Components();
  std::vector<StripeColumn>& col_list = *output_stripes_;
  int sz = (int)component_list.size();
  std::vector<int> component_column_assign(sz, -1);
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list[i].component_count_ = 0;
    col_list[i].component_list_.clear();
  }

  for (int i = 0; i < sz; ++i) {
    if (component_list[i].IsFixed()) continue;
    int col_num = LocToCol((int)std::round(component_list[i].X()));

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

    Stripe* stripe = nullptr;
    double min_dist = DBL_MAX;
    for (auto& num : pos_col) {
      double tmp_dist;
      Stripe* res = col_list[num].GetStripeClosestToComponent(
          &component_list[i], tmp_dist);
      if (tmp_dist < min_dist) {
        stripe = res;
        col_num = num;
        min_dist = tmp_dist;
      }
    }
    if (stripe != nullptr) {
      col_list[col_num].component_count_++;
      component_column_assign[i] = col_num;
    } else {
      DaliExpects(false, "Cannot find a column to place component: " +
                             component_list[i].Name());
    }
  }
  for (int i = 0; i < tot_col_num_; ++i) {
    int capacity = col_list[i].component_count_;
    col_list[i].component_list_.reserve(capacity);
  }

  for (int i = 0; i < sz; ++i) {
    if (component_list[i].IsFixed()) continue;
    int col_num = component_column_assign[i];
    if (col_num >= 0) {
      col_list[col_num].component_list_.push_back(&component_list[i]);
    }
  }

  for (auto& col : col_list) {
    col.AssignComponentToSimpleStripe();
  }
}

/**
 * Partition the region into stripe columns and assign components to them.
 *
 * Fetches the well rules, chooses column boundaries within the maximum row
 * width, distributes components by whitespace, and cuts each column into stripes.
 * @return true on success.
 */
bool WellSpacePartitioner::StartPartitioning() {
  DaliExpects(circuit_ != nullptr, "Circuit is not set");
  DaliExpects(output_stripes_ != nullptr, "Output location is not set");

  // Partitioning is reused by provisional legalization during global
  // placement. Clear all output and row caches before rebuilding them from
  // the current component locations.
  output_stripes_->clear();
  white_space_in_rows_.clear();

  DetectAvailSpace();

  FetchWellParameters();

  std::vector<StripeColumn>& col_list = *output_stripes_;
  // find the maximum width among movable cells
  max_component_width_ = 0;
  for (auto& component : circuit_->Components()) {
    if (component.IsMovable()) {
      max_component_width_ = std::max(max_component_width_, component.Width());
    }
  }
  LOG(info) << "Max movable component width: " << max_component_width_ << "\n";

  // determine the width of columns
  cluster_width_ = max_row_width_;
  if (cluster_width_ <= 0) {
    LOG(info) << "Using default gridded row width: 2*max_unplug_length_\n";
    stripe_width_ = (int)std::round(max_unplug_length_ * stripe_width_factor_);
  } else {
    // A row narrower than MaxPlugDist is fine -- even preferred -- for latch-up
    // verifier checks it geometrically). The only width that can actually block
    // legalization is one smaller than the widest movable cell.
    DaliWarns(cluster_width_ < max_component_width_,
              "Specified gridded row width is smaller than the widest movable "
              "cell and cannot legalize it into a row this narrow");
    stripe_width_ = cluster_width_;
  }
  stripe_width_ = stripe_width_ + well_spacing_;
  int region_width = Right() - Left();
  int region_height = Top() - Bottom();
  if (stripe_width_ > region_width) {
    stripe_width_ = region_width;
  }
  tot_col_num_ = std::ceil(region_width / (double)stripe_width_);
  LOG(info) << "  Total number of columns: " << tot_col_num_ << "\n";
  int max_clusters_per_col = region_height / circuit_->MinComponentHeight();
  col_list.resize(tot_col_num_);
  stripe_width_ = region_width / tot_col_num_;
  LOG(info) << "  Gridded row width: " << stripe_width_ * circuit_->GridValueX()
            << "um, " << stripe_width_ << "\n";
  DaliWarns(stripe_width_ < max_component_width_,
            "Maximum component width is longer than gridded row width?");
  std::vector<int> column_boundaries = PlanColumnBoundaries(region_width);
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list[i].lx_ = column_boundaries[i];
    col_list[i].width_ =
        column_boundaries[i + 1] - column_boundaries[i] - well_spacing_;
    DaliExpects(col_list[i].width_ > 0,
                "CELL configuration is problematic, leading to non-positive "
                "column width");
    UpdateWhiteSpaceInCol(col_list[i]);
  }
  if (partition_mode_ == 1) {
    col_list.back().width_ = Right() - col_list.back().lx_;
    UpdateWhiteSpaceInCol(col_list.back());
  }
  DecomposeSpaceToSimpleStripes();

  LOG(info) << "Maximum possible number of gridded rows in a column: "
            << max_clusters_per_col << "\n";

  AssignComponentToColBasedOnWhiteSpace();

  return true;
}

int WellSpacePartitioner::Left() const {
  return circuit_->RegionLLX() + l_space_;
}

int WellSpacePartitioner::Right() const {
  return circuit_->RegionURX() - r_space_;
}

int WellSpacePartitioner::Bottom() const {
  return circuit_->RegionLLY() + b_space_;
}

int WellSpacePartitioner::Top() const {
  return circuit_->RegionURY() - t_space_;
}

int WellSpacePartitioner::StartRow(int y_loc) const {
  return (y_loc - Bottom()) / row_height_;
}

int WellSpacePartitioner::EndRow(int y_loc) const {
  int relative_y = y_loc - Bottom();
  int res = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --res;
  }
  return res;
}

int WellSpacePartitioner::RowToLoc(int row_num, int displacement) const {
  return row_num * row_height_ + Bottom() + displacement;
}

int WellSpacePartitioner::LocToCol(int x) const {
  const std::vector<StripeColumn>& columns = *output_stripes_;
  auto column =
      std::upper_bound(columns.begin(), columns.end(), x,
                       [](int location, const StripeColumn& candidate) {
                         return location < candidate.LLX();
                       });
  if (column == columns.begin()) return 0;
  return static_cast<int>(std::distance(columns.begin(), column) - 1);
}

/**
 * Choose the column boundary coordinates across the region.
 *
 * Columns are separated by well spacing so each is an independent well region,
 * and no wider than the technology's maximum row width.
 * @return the boundary X coordinates.
 */
std::vector<int> WellSpacePartitioner::PlanColumnBoundaries(
    int region_width) const {
  std::vector<int> uniform_boundaries(tot_col_num_ + 1);
  for (int column = 0; column <= tot_col_num_; ++column) {
    uniform_boundaries[column] = Left() + stripe_width_ * column;
  }
  if (!column_boundaries_override_.empty()) {
    DaliExpects(column_boundaries_override_.size() ==
                    static_cast<size_t>(tot_col_num_ + 1),
                "Explicit stripe boundary count does not match column count");
    DaliExpects(column_boundaries_override_.front() == Left(),
                "Explicit stripe boundaries do not start at region left");
    DaliExpects(column_boundaries_override_.back() <= Right(),
                "Explicit stripe boundaries exceed region right");
    DaliExpects(std::adjacent_find(column_boundaries_override_.begin(),
                                   column_boundaries_override_.end(),
                                   [](int lhs, int rhs) {
                                     return lhs >= rhs;
                                   }) == column_boundaries_override_.end(),
                "Explicit stripe boundaries must be strictly increasing");
    return column_boundaries_override_;
  }
  if (!use_adaptive_boundaries_ || tot_col_num_ == 1) {
    return uniform_boundaries;
  }

  const int average_pitch = region_width / tot_col_num_;
  AdaptiveStripeBoundaryConfig planner_config;
  planner_config.region_left = Left();
  planner_config.region_right = Right();
  planner_config.column_count = tot_col_num_;
  planner_config.minimum_column_pitch = std::max(
      max_component_width_ + well_spacing_ + capacity_config_.reserved_width,
      average_pitch / 2);
  planner_config.maximum_column_pitch =
      std::min(region_width, average_pitch * 3 / 2);
  planner_config.boundary_step = std::max(1, max_component_width_);
  planner_config.spacing_per_column =
      well_spacing_ + capacity_config_.reserved_width;

  std::map<std::vector<int>, int> signature_ids;
  std::vector<StripePackingSample> samples;
  samples.reserve(circuit_->Components().size());
  for (const Component& component : circuit_->Components()) {
    if (!component.IsMovable()) continue;
    const Macro* macro = component.MacroPtr();
    DaliExpects(macro != nullptr, "Movable component has no cell master");

    std::vector<int> signature;
    int signature_height = 0;
    if (macro->HasCompleteWellRegions()) {
      signature.reserve(2 * macro->RegionCount());
      for (int region = 0; region < macro->RegionCount(); ++region) {
        int p_height =
            std::max(macro->PwellHeight(region, component.IsFlipped()),
                     capacity_config_.minimum_p_well_height);
        int n_height =
            std::max(macro->NwellHeight(region, component.IsFlipped()),
                     capacity_config_.minimum_n_well_height);
        signature.push_back(p_height);
        signature.push_back(n_height);
        signature_height += p_height + n_height;
      }
    } else {
      int p_height = std::max(macro->FirstPwellHeight(),
                              capacity_config_.minimum_p_well_height);
      int n_height = std::max(macro->FirstNwellHeight(),
                              capacity_config_.minimum_n_well_height);
      signature = {p_height, n_height};
      signature_height = p_height + n_height;
    }
    auto [signature_entry, inserted] = signature_ids.emplace(
        std::move(signature), static_cast<int>(signature_ids.size()));
    (void)inserted;
    samples.push_back({component.X(), component.Width(), signature_height,
                       signature_entry->second});
  }

  AdaptiveStripeBoundaryResult plan =
      PackedStripeBoundaryPlanner(planner_config).Plan(samples);
  if (!plan.feasible) {
    LOG(warning)
        << "  Adaptive stripe planning failed; use uniform boundaries\n";
    return uniform_boundaries;
  }

  if (adaptive_boundary_blend_ < 1.0) {
    for (int column = 1; column < tot_col_num_; ++column) {
      int uniform_boundary =
          Left() +
          static_cast<int>(std::llround(region_width * column /
                                        static_cast<double>(tot_col_num_)));
      plan.boundaries[column] = static_cast<int>(std::llround(
          uniform_boundary + adaptive_boundary_blend_ *
                                 (plan.boundaries[column] - uniform_boundary)));
    }
  }

  int minimum_pitch = region_width;
  int maximum_pitch = 0;
  for (size_t i = 1; i < plan.boundaries.size(); ++i) {
    int pitch = plan.boundaries[i] - plan.boundaries[i - 1];
    minimum_pitch = std::min(minimum_pitch, pitch);
    maximum_pitch = std::max(maximum_pitch, pitch);
  }
  LOG(info) << "  Packed adaptive stripe boundaries:\n"
            << "    objective       : " << plan.objective << "\n"
            << "    row signatures  : " << signature_ids.size() << "\n"
            << "    adaptive blend  : " << adaptive_boundary_blend_ << "\n"
            << "    pitch range     : "
            << minimum_pitch * circuit_->GridValueX() << "-"
            << maximum_pitch * circuit_->GridValueX() << "um\n";
  return plan.boundaries;
}

}  // namespace dali
