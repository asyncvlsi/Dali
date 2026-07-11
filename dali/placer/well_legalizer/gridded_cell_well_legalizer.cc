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
#include "gridded_cell_well_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "dali/common/helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/stripe_helper.h"
#include "dali/placer/well_legalizer/well_geometry.h"
#include "dali/placer/well_legalizer/well_geometry_exporter.h"

namespace dali {

GriddedCellWellLegalizer::GriddedCellWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_width_ = 0;
}

void GriddedCellWellLegalizer::SetSnapshotCallback(
    SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void GriddedCellWellLegalizer::LoadConf(std::string const& config_file) {
  config_read(config_file.c_str());
  DaliExpects(false, "Not implemented");
}

void GriddedCellWellLegalizer::CheckWellStatus() {
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (component.IsMovable()) {
      DaliExpects(component.MacroPtr()->HasWellInfo(),
                  "Cannot find well info for component: " << component.Name());
    }
  }
}

void GriddedCellWellLegalizer::FetchNpWellParams() {
  Tech& tech = ckt_ptr_->tech();
  WellLayer& n_well_layer = tech.NwellLayer();
  double grid_value_x = ckt_ptr_->GridValueX();
  int same_well_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_well_spacing =
      std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ =
      (int)std::floor(n_well_layer.MaxPlugDist() / grid_value_x);
  DaliExpects(!ckt_ptr_->tech().WellTapMacroIds().empty(),
              "Cannot find the definition of well tap cell, well legalization "
              "cannot proceed\n");
  int well_tap_macro_id = ckt_ptr_->tech().WellTapMacroIds()[0];
  well_tap_macro_ = &(ckt_ptr_->tech().Macros()[well_tap_macro_id]);
  well_tap_width_ = well_tap_macro_->Width();

  LOG(info) << "  Well max plug distance: " << n_well_layer.MaxPlugDist()
            << "um, " << max_unplug_length_ << " \n";
  LOG(info) << "  GridValueX: " << ckt_ptr_->GridValueX() << " um\n";
  LOG(info) << "  Well spacing: " << n_well_layer.Spacing() << "um, "
            << well_spacing_ << "\n";
  LOG(info) << "  Well tap cell width: " << well_tap_width_ << "\n";

  if (enable_end_cap_cell_) {
    pre_end_cap_min_width_ = ckt_ptr_->tech().PreEndCapMinWidth();
    LOG(info) << "  pre_end_cap_min_width: " << pre_end_cap_min_width_ << "\n";

    pre_end_cap_min_p_height_ = ckt_ptr_->tech().PreEndCapMinPHeight();
    LOG(info) << "  pre_end_cap_min_p_height: " << pre_end_cap_min_p_height_
              << "\n";

    pre_end_cap_min_n_height_ = ckt_ptr_->tech().PreEndCapMinNHeight();
    LOG(info) << "  pre_end_cap_min_n_height: " << pre_end_cap_min_n_height_
              << "\n";

    post_end_cap_min_width_ = ckt_ptr_->tech().PostEndCapMinWidth();
    LOG(info) << "  post_end_cap_min_width: " << post_end_cap_min_width_
              << "\n";

    post_end_cap_min_p_height_ = ckt_ptr_->tech().PostEndCapMinPHeight();
    LOG(info) << "  post_end_cap_min_p_height: " << post_end_cap_min_p_height_
              << "\n";

    post_end_cap_min_n_height_ = ckt_ptr_->tech().PostEndCapMinNHeight();
    LOG(info) << "  post_end_cap_min_n_height: " << post_end_cap_min_n_height_
              << "\n";

    EnsureUsableEndCapWidths();
  }

  well_tap_p_height_ = well_tap_macro_->FirstPwellHeight();
  well_tap_n_height_ = well_tap_macro_->FirstNwellHeight();
}

void GriddedCellWellLegalizer::SaveInitialComponentLocation() {
  component_init_locations_ = CaptureComponentPlacement();
}

std::vector<GriddedCellWellLegalizer::ComponentPlacementSnapshot>
GriddedCellWellLegalizer::CaptureComponentPlacement() const {
  std::vector<ComponentPlacementSnapshot> component_snapshots;
  const std::vector<Component>& component_list = ckt_ptr_->Components();
  component_snapshots.reserve(component_list.size());
  for (auto& component : component_list) {
    ComponentPlacementSnapshot snapshot;
    snapshot.lx = component.LLX();
    snapshot.ly = component.LLY();
    snapshot.orient = component.Orient();
    component_snapshots.push_back(snapshot);
  }
  return component_snapshots;
}

void GriddedCellWellLegalizer::RestoreComponentPlacement(
    const std::vector<ComponentPlacementSnapshot>& component_snapshots) {
  DaliExpects(component_snapshots.size() == ckt_ptr_->Components().size(),
              "Cannot restore component placement: component count changed");

  std::vector<Component>& component_list = ckt_ptr_->Components();
  for (size_t i = 0; i < component_list.size(); ++i) {
    const ComponentPlacementSnapshot& snapshot = component_snapshots[i];
    component_list[i].SetLLX(snapshot.lx);
    component_list[i].SetLLY(snapshot.ly);
    component_list[i].SetOrient(snapshot.orient);
  }
}

void GriddedCellWellLegalizer::RestoreInitialComponentLocation() {
  RestoreComponentPlacement(component_init_locations_);
}

void GriddedCellWellLegalizer::SetMaxRowWidth(double max_row_width_microns) {
  if (max_row_width_microns < 0) {
    max_row_width_ = -1;
    return;
  }
  DaliExpects(ckt_ptr_ != nullptr, "Circuit must be set before row width");
  max_row_width_ = std::floor(max_row_width_microns / ckt_ptr_->GridValueX());
  LOG(info) << "Max row width in grid unit : " << max_row_width_ << "\n";
}

void GriddedCellWellLegalizer::InitializeWellLegalizer(int cluster_width) {
  if (disable_welltap_) {
    well_tap_count_per_cluster_ = 0;
    LOG(info) << "set number of tap cells to 0, since well tap is disabled\n";
  }

  CheckWellStatus();

  // fetch parameters related to N/P-well
  FetchNpWellParams();

  space_partitioner_.SetCircuit(ckt_ptr_);
  space_partitioner_.SetOutput(&col_list_);
  if (disable_welltap_) {
    space_partitioner_.SetReservedSpaceToBoundaries(0, 0, 0, 0);
  } else {
    space_partitioner_.SetReservedSpaceToBoundaries(well_spacing_,
                                                    well_spacing_, 1, 1);
  }
  space_partitioner_.SetPartitionMode(stripe_mode_);
  if (cluster_width >= 0) {
    space_partitioner_.SetMaxRowWidth(cluster_width);
  } else {
    space_partitioner_.SetMaxRowWidth(max_row_width_);
  }
  space_partitioner_.StartPartitioning();

  index_loc_list_.resize(ckt_ptr_->Components().size());
}

int GriddedCellWellLegalizer::PhysicalCompletionReservedWidth() const {
  int reserved_width = 0;
  if (!disable_welltap_) {
    reserved_width = well_tap_count_per_cluster_ * well_tap_width_ +
                     well_tap_count_per_cluster_ * space_to_well_tap_;
  }
  if (enable_end_cap_cell_) {
    reserved_width += pre_end_cap_min_width_ + post_end_cap_min_width_;
  }
  return reserved_width;
}

void GriddedCellWellLegalizer::ReservePhysicalCompletionSpace(
    GriddedRow* row, bool grows_upward) {
  if (row == nullptr) {
    return;
  }

  if (grows_upward) {
    row->UpdateWellHeightUpward(well_tap_p_height_, well_tap_n_height_);
    if (enable_end_cap_cell_) {
      row->UpdateWellHeightUpward(
          std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_),
          std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
    }
  } else {
    row->UpdateWellHeightDownward(well_tap_p_height_, well_tap_n_height_);
    if (enable_end_cap_cell_) {
      row->UpdateWellHeightDownward(
          std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_),
          std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
    }
  }
}

void GriddedCellWellLegalizer::EnsureUsableEndCapWidths() {
  if (pre_end_cap_min_width_ > 0 && post_end_cap_min_width_ > 0) {
    return;
  }

  int fallback_width = well_tap_width_;
  if (pre_end_cap_min_width_ <= 0) {
    LOG(warning) << "  pre-end-cap width is not provided; use "
                 << fallback_width
                 << " grid units for generated end-cap cells\n";
    pre_end_cap_min_width_ = fallback_width;
  }
  if (post_end_cap_min_width_ <= 0) {
    LOG(warning) << "  post-end-cap width is not provided; use "
                 << fallback_width
                 << " grid units for generated end-cap cells\n";
    post_end_cap_min_width_ = fallback_width;
  }
}

int GriddedCellWellLegalizer::LeftTapLx(const Stripe& stripe) const {
  int end_cap_width = enable_end_cap_cell_ ? pre_end_cap_min_width_ : 0;
  return stripe.LLX() + end_cap_width;
}

int GriddedCellWellLegalizer::LeftTapUx(const Stripe& stripe) const {
  return LeftTapLx(stripe) + well_tap_width_;
}

int GriddedCellWellLegalizer::RightTapUx(const Stripe& stripe) const {
  int end_cap_width = enable_end_cap_cell_ ? post_end_cap_min_width_ : 0;
  return stripe.URX() - end_cap_width;
}

int GriddedCellWellLegalizer::RightTapLx(const Stripe& stripe) const {
  return RightTapUx(stripe) - well_tap_width_;
}

WellRowCompletionConfig GriddedCellWellLegalizer::BuildRowCompletionConfig()
    const {
  WellRowCompletionConfig config;
  config.well_tap_macro = well_tap_macro_;
  config.well_tap_count_per_row = well_tap_count_per_cluster_;
  config.space_to_well_tap = space_to_well_tap_;
  if (enable_end_cap_cell_) {
    config.pre_end_cap_width = pre_end_cap_min_width_;
    config.post_end_cap_width = post_end_cap_min_width_;
  }
  return config;
}

void GriddedCellWellLegalizer::CreateClusterAndAppendSingleWellComponent(
    Stripe& stripe, Component& component) {
  stripe.gridded_rows_.emplace_back();
  GriddedRow* front_row = &(stripe.gridded_rows_.back());
  front_row->Components().reserve(stripe.max_component_capacity_per_cluster_);
  front_row->AddComponent(&component);

  int width = component.Width();
  int init_y = (int)std::round(component.LLY());
  init_y = std::max(init_y, stripe.contour_);

  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();

  front_row->SetUsedSize(PhysicalCompletionReservedWidth() + width);
  ReservePhysicalCompletionSpace(front_row, true);
  // row height should be able to accommodate ordinary cell
  front_row->UpdateWellHeightUpward(p_well_height, n_well_height);
  front_row->SetLLY(init_y);
  front_row->SetLLX(stripe.LLX());
  front_row->SetWidth(stripe.Width());

  stripe.front_row_ = front_row;
  stripe.cluster_count_ += 1;
  stripe.used_height_ += front_row->Height();
  stripe.contour_ = front_row->URY();
}

void GriddedCellWellLegalizer::AppendSingleWellComponentToFrontCluster(
    Stripe& stripe, Component& component) {
  int width = component.Width();
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();

  GriddedRow* front_row = stripe.front_row_;
  front_row->AddComponent(&component);
  front_row->UseSpace(width);
  if (p_well_height > front_row->PHeight() ||
      n_well_height > front_row->NHeight()) {
    int old_height = front_row->Height();
    front_row->UpdateWellHeightUpward(p_well_height, n_well_height);
    stripe.used_height_ += front_row->Height() - old_height;
  }
  stripe.contour_ = front_row->URY();
}

void GriddedCellWellLegalizer::AppendComponentToColBottomUp(
    Stripe& stripe, Component& component) {
  bool is_no_row_in_col = (stripe.contour_ == stripe.LLY());
  bool is_new_row_needed = is_no_row_in_col;
  if (!is_new_row_needed) {
    GriddedRow* front_row = stripe.front_row_;
    bool is_not_in_top_row = stripe.contour_ <= component.LLY();
    bool is_top_row_full =
        front_row->UsedSize() + component.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  if (is_new_row_needed) {
    CreateClusterAndAppendSingleWellComponent(stripe, component);
  } else {
    AppendSingleWellComponentToFrontCluster(stripe, component);
  }
}

void GriddedCellWellLegalizer::AppendComponentToColTopDown(
    Stripe& stripe, Component& component) {
  bool is_no_row = stripe.gridded_rows_.empty();
  bool is_new_row_needed = is_no_row;
  if (!is_new_row_needed) {
    bool is_not_in_top_row = stripe.contour_ >= component.URY();
    bool is_top_row_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow* front_row;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_row_needed) {
    stripe.gridded_rows_.emplace_back();
    front_row = &(stripe.gridded_rows_.back());
    front_row->Components().reserve(stripe.max_component_capacity_per_cluster_);
    front_row->AddComponent(&component);
    front_row->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_row, false);
    front_row->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_row->SetURY(init_y);
    front_row->SetLLX(stripe.LLX());
    front_row->SetWidth(stripe.Width());

    stripe.front_row_ = front_row;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_row->Height();
  } else {
    front_row = stripe.front_row_;
    front_row->AddComponent(&component);
    front_row->UseSpace(width);
    if (p_well_height > front_row->PHeight() ||
        n_well_height > front_row->NHeight()) {
      int old_height = front_row->Height();
      front_row->UpdateWellHeightDownward(p_well_height, n_well_height);
      stripe.used_height_ += front_row->Height() - old_height;
    }
  }
  stripe.contour_ = front_row->LLY();
}

void GriddedCellWellLegalizer::AppendComponentToColBottomUpCompact(
    Stripe& stripe, Component& component) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.LLY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.LLY());
  init_y = std::max(init_y, stripe.contour_);

  GriddedRow* front_cluster;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Components().reserve(
        stripe.max_component_capacity_per_cluster_);
    front_cluster->AddComponent(&component);
    front_cluster->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_cluster, true);
    front_cluster->SetLLY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());
    front_cluster->UpdateWellHeightUpward(p_well_height, n_well_height);

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddComponent(&component);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() ||
        n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightUpward(p_well_height, n_well_height);
      stripe.used_height_ += front_cluster->Height() - old_height;
    }
  }
  stripe.contour_ = front_cluster->URY();
}

void GriddedCellWellLegalizer::AppendComponentToColTopDownCompact(
    Stripe& stripe, Component& component) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.URY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow* front_cluster;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Components().reserve(
        stripe.max_component_capacity_per_cluster_);
    front_cluster->AddComponent(&component);
    front_cluster->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_cluster, false);
    front_cluster->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_cluster->SetURY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddComponent(&component);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() ||
        n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightDownward(p_well_height, n_well_height);
      stripe.used_height_ += front_cluster->Height() - old_height;
    }
  }
  stripe.contour_ = front_cluster->LLY();
}

bool GriddedCellWellLegalizer::StripeLegalizationBottomUp(Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.LLY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColBottomUp(stripe, *component_ptr);
  }

  for (auto& gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateComponentLocY();
  }

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedCellWellLegalizer::StripeLegalizationTopDown(Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = false;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColTopDown(stripe, *component_ptr);
  }

  for (auto& gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateComponentLocY();
  }

  /*LOG(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    LOG(info)   << "success\n";
  } else {
    LOG(info)   << "fail\n";
  }*/

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedCellWellLegalizer::StripeLegalizationBottomUpCompact(
    Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = RegionBottom();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColBottomUpCompact(stripe, *component_ptr);
  }

  for (auto& cluster : stripe.gridded_rows_) {
    cluster.UpdateComponentLocY();
  }

  return stripe.contour_ <= RegionTop();
}

bool GriddedCellWellLegalizer::StripeLegalizationTopDownCompact(Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = false;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColTopDownCompact(stripe, *component_ptr);
  }

  for (auto& cluster : stripe.gridded_rows_) {
    cluster.UpdateComponentLocY();
  }

  /*LOG(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    LOG(info)   << "success\n";
  } else {
    LOG(info)   << "fail\n";
  }*/

  return stripe.contour_ >= RegionBottom();
}

bool GriddedCellWellLegalizer::ComponentClustering() {
  /****
   * Clustering components in each stripe
   * After clustering, close pack clusters from bottom to top
   ****/
  bool res = true;
  for (auto& col : col_list_) {
    bool is_success = true;
    for (auto& stripe : col.stripe_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripeLegalizationBottomUp(stripe);
        if (!is_success) {
          is_success = StripeLegalizationTopDown(stripe);
        }
      }
      res = res && is_success;

      // closely pack clusters from bottom to top
      stripe.contour_ = stripe.LLY();
      if (stripe.is_bottom_up_) {
        for (auto& cluster : stripe.gridded_rows_) {
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateComponentLocY();
          cluster.LegalizeCompactX();
        }
      } else {
        int sz = static_cast<int>(stripe.gridded_rows_.size());
        for (int i = sz - 1; i >= 0; --i) {
          auto& cluster = stripe.gridded_rows_[i];
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateComponentLocY();
          cluster.LegalizeCompactX();
        }
      }
    }
  }
  return res;
}

/****
 * Clustering components in each stripe
 * After clustering, leave clusters as they are
 * ****/
bool GriddedCellWellLegalizer::ComponentClusteringLoose() {
  int step = 50;
  int count = 0;
  bool res = true;
  int failed_stripe_count = 0;
  for (int col_id = 0; col_id < static_cast<int>(col_list_.size()); ++col_id) {
    auto& col = col_list_[col_id];
    bool is_success = true;
    for (int stripe_id = 0;
         stripe_id < static_cast<int>(col.stripe_list_.size()); ++stripe_id) {
      auto& stripe = col.stripe_list_[stripe_id];
      int i = 0;
      bool is_from_bottom = true;
      for (i = 0; i < max_iter_; ++i) {
        if (is_from_bottom) {
          is_success = StripeLegalizationBottomUp(stripe);
        } else {
          is_success = StripeLegalizationTopDown(stripe);
        }
        if (!is_success) {
          is_success = TrialClusterLegalization(stripe);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
      if (!is_success) {
        ++failed_stripe_count;
        LogStripeLegalizationFailure(col, stripe, col_id, stripe_id);
      }
      /*if (is_success) {
        LOG(info)  <<"stripe legalization success, %d\n", i);
      } else {
        LOG(info)  <<"stripe legalization fail, %d\n", i);
      }*/

      for (auto& row : stripe.gridded_rows_) {
        row.UpdateComponentLocY();
        row.MinDisplacementLegalization();
        if (is_dump) {
          if (count % step == 0) {
            std::string tmp_file_name =
                "wlg_result_" + std::to_string(dump_count) + ".txt";
            ckt_ptr_->GenMATLABTable(tmp_file_name);
            ++dump_count;
          }
          ++count;
        }
      }
      stripe.MinDisplacementAdjustment();
    }
  }

  LogComponentClusteringSummary(failed_stripe_count);
  return res;
}

void GriddedCellWellLegalizer::LogStripeLegalizationFailure(
    const StripeColumn& col, const Stripe& stripe, int column_index,
    int stripe_index) const {
  int lowest_row_y = std::numeric_limits<int>::max();
  int highest_row_y = std::numeric_limits<int>::min();
  for (const auto& row : stripe.gridded_rows_) {
    lowest_row_y = std::min(lowest_row_y, row.LLY());
    highest_row_y = std::max(highest_row_y, row.URY());
  }
  if (stripe.gridded_rows_.empty()) {
    lowest_row_y = stripe.LLY();
    highest_row_y = stripe.LLY();
  }

  int lower_overflow = std::max(0, stripe.LLY() - lowest_row_y);
  int upper_overflow = std::max(0, highest_row_y - stripe.URY());
  int height_overflow = std::max(0, stripe.used_height_ - stripe.Height());
  double grid_y = ckt_ptr_->GridValueY();

  LOG(warning) << "  stripe legalization failed:"
               << " col=" << column_index << " stripe=" << stripe_index
               << " col_x=[" << col.LLX() << ", " << col.URX() << ")"
               << " stripe_box=[" << stripe.LLX() << ", " << stripe.LLY()
               << "]-[" << stripe.URX() << ", " << stripe.URY() << ")"
               << " components=" << stripe.component_ptrs_vec_.size()
               << " rows=" << stripe.gridded_rows_.size()
               << " used_height=" << stripe.used_height_
               << " capacity_height=" << stripe.Height()
               << " overflow_height=" << height_overflow << " row_y=["
               << lowest_row_y << ", " << highest_row_y << ")"
               << " lower_overflow=" << lower_overflow
               << " upper_overflow=" << upper_overflow << " grid units, "
               << "overflow_height=" << height_overflow * grid_y << "um\n";
}

void GriddedCellWellLegalizer::LogComponentClusteringSummary(
    int failed_stripe_count) const {
  size_t overlap_count = CountComponentOverlapsInRows();
  if (failed_stripe_count == 0) {
    LOG(info) << "  component clustering: all stripes legalized\n";
    if (overlap_count > 0) {
      LOG(warning) << "  component clustering produced " << overlap_count
                   << " overlapping component pair(s)\n";
    }
    return;
  }
  LOG(warning) << "  component clustering failed in " << failed_stripe_count
               << " stripe(s)\n";
  if (overlap_count > 0) {
    LOG(warning) << "  component clustering has " << overlap_count
                 << " overlapping component pair(s)\n";
  }
}

size_t GriddedCellWellLegalizer::CountComponentOverlapsInRows() const {
  size_t overlap_count = 0;
  for (const auto& col : col_list_) {
    for (const auto& stripe : col.stripe_list_) {
      for (const auto& row : stripe.gridded_rows_) {
        overlap_count += row.CountComponentOverlaps();
      }
    }
  }
  return overlap_count;
}

bool GriddedCellWellLegalizer::ComponentClusteringCompact() {
  /****
   * Clustering components in each stripe in a compact way
   * After clustering, leave clusters as they are
   * ****/

  bool res = true;
  for (auto& col : col_list_) {
    bool is_success = true;
    for (auto& stripe : col.stripe_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripeLegalizationBottomUpCompact(stripe);
        if (!is_success) {
          is_success = StripeLegalizationTopDownCompact(stripe);
        }
      }
      res = res && is_success;

      for (auto& cluster : stripe.gridded_rows_) {
        cluster.UpdateComponentLocY();
        cluster.LegalizeLooseX();
      }
    }
  }

  return res;
}

bool GriddedCellWellLegalizer::TrialClusterLegalization(Stripe& stripe) {
  /****
   * Legalize the location of all clusters using extended Tetris legalization
   * algorithm in columns where usage does not exceed capacity Closely pack the
   * column from bottom to top if its usage exceeds its capacity
   * ****/

  bool res = true;

  // sort clusters in each column based on the lower left coordinate
  std::vector<GriddedRow*> cluster_list;
  cluster_list.resize(stripe.cluster_count_, nullptr);
  for (int i = 0; i < stripe.cluster_count_; ++i) {
    cluster_list[i] = &stripe.gridded_rows_[i];
  }

  // LOG(info)   << "used height/RegionHeight(): " <<
  // col.used_height_ / (double) RegionHeight() << "\n";
  if (stripe.used_height_ <= RegionHeight()) {
    if (stripe.is_bottom_up_) {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow* lhs, const GriddedRow* rhs) {
                  return (lhs->URY() > rhs->URY());
                });
      int cluster_contour = stripe.URY();
      int res_y;
      int init_y;
      for (auto& cluster : cluster_list) {
        init_y = cluster->URY();
        res_y = std::min(cluster_contour, cluster->URY());
        cluster->SetURY(res_y);
        cluster_contour = cluster->LLY();
        cluster->ShiftComponentY(res_y - init_y);
      }
    } else {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow* lhs, const GriddedRow* rhs) {
                  return (lhs->LLY() < rhs->LLY());
                });
      int cluster_contour = stripe.LLY();
      int res_y;
      int init_y;
      for (auto& cluster : cluster_list) {
        init_y = cluster->LLY();
        res_y = std::max(cluster_contour, cluster->LLY());
        cluster->SetLLY(res_y);
        cluster_contour = cluster->URY();
        cluster->ShiftComponentY(res_y - init_y);
      }
    }
  } else {
    std::sort(cluster_list.begin(), cluster_list.end(),
              [](const GriddedRow* lhs, const GriddedRow* rhs) {
                return (lhs->LLY() < rhs->LLY());
              });
    int cluster_contour = RegionBottom();
    int res_y;
    int init_y;
    for (auto& cluster : cluster_list) {
      init_y = cluster->LLY();
      res_y = cluster_contour;
      cluster->SetLLY(res_y);
      cluster_contour += cluster->Height();
      cluster->ShiftComponentY(res_y - init_y);
    }
    res = false;
  }

  return res;
}

/*
void GriddedCellWellLegalizer::SingleSegmentClusteringOptimization() {
  LOG(info) << "Start single segment clustering\n";

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.cluster_list_) {
        int old_cluster_component_count = cluster.components_.size();
        std::vector<ComponentSegment> old_cluster(old_cluster_component_count);
        for (int i = 0; i < old_cluster_component_count; ++i) {
          old_cluster[i].component_index.push_back(i);
          old_cluster[i].circuit_ptr_ = circuit_ptr_;
          old_cluster[i].cluster_ = &cluster;
          old_cluster[i].UpdateBoundList();
          old_cluster[i].SortBounds();
          old_cluster[i].UpdateLLX();
        }

        bool is_overlap = false;
        do {
          std::vector<ComponentSegment> new_cluster;
          new_cluster.push_back(old_cluster[0]);
          int j = 0;
          while (j + 1 < old_cluster_component_count) {
            if (new_cluster.back().Overlap(old_cluster[j + 1])) {
              new_cluster.back().Merge(old_cluster[j + 1]);
            } else {
              new_cluster.push_back(old_cluster[j + 1]);
            }
            j += 1;
          }
          int new_count = new_cluster.size();
          old_cluster_component_count = new_count;
          for (int i = 0; i < new_count; ++i) {
            old_cluster[i].CopyFrom(new_cluster[i]);
          }

          is_overlap = false;
          for (int i = 0; i < new_count - 1; ++i) {
            if (old_cluster[i].IsNotOnLeft(old_cluster[i + 1])) {
              is_overlap = true;
              break;
            }
          }

          //LOG(info)   << is_overlap << "\n";

        } while (is_overlap);

        for (int i = 0; i < old_cluster_component_count; ++i) {
          old_cluster[i].UpdateComponentLocation();
        }
      }
    }
  }

}
 */

void GriddedCellWellLegalizer::UpdateClusterOrient() {
  for (auto& col : col_list_) {
    bool is_orient_N = is_first_row_orient_N_;
    for (auto& stripe : col.stripe_list_) {
      if (stripe.is_bottom_up_) {
        for (auto& cluster : stripe.gridded_rows_) {
          cluster.SetOrient(is_orient_N);
          is_orient_N = !is_orient_N;
        }
      } else {
        int sz = stripe.gridded_rows_.size();
        for (int i = sz - 1; i >= 0; --i) {
          stripe.gridded_rows_[i].SetOrient(is_orient_N);
          is_orient_N = !is_orient_N;
        }
      }
    }
  }
}

void GriddedCellWellLegalizer::ClearCachedData() {
  for (auto& component : ckt_ptr_->Components()) {
    component.SetOrient(N);
  }

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      stripe.contour_ = stripe.LLY();
      stripe.used_height_ = 0;
      stripe.cluster_count_ = 0;
      stripe.gridded_rows_.clear();
      stripe.front_row_ = nullptr;
    }
  }

  // cluster_list_.clear();
}

bool GriddedCellWellLegalizer::WellLegalize() {
  bool is_success = true;
  InitializeWellLegalizer();
  is_success = ComponentClusteringLoose();
  // ComponentClusteringCompact();
  ReportHPWL();

  if (is_success) {
    LOG(info) << "\033[0;36m"
              << "Standard Cluster Well Legalization complete!\n"
              << "\033[0m";
  } else {
    LOG(info) << "\033[0;36m"
              << "Standard Cluster Well Legalization fail!\n"
              << "\033[0m";
  }

  return is_success;
}

bool GriddedCellWellLegalizer::RunComponentClusteringStage() {
  LOG(info) << "Form component clustering\n";
  bool is_success = ComponentClusteringLoose();
  ReportHPWL();
  RecordPlacementMetric("well_legalization.component_clustering",
                        WeightedHPWL());
  EmitSnapshot("component_clustering", "After Component Clustering",
               "legalization", "component_clustering");
  return is_success;
}

void GriddedCellWellLegalizer::RunClusterOrientationStage() {
  if (disable_cell_flip_) {
    LOG(info) << "Skip flipping cluster orientation\n";
    return;
  }
  LOG(info) << "Flip cluster orientation\n";
  UpdateClusterOrient();
  ReportHPWL();
  RecordPlacementMetric("well_legalization.orientation", WeightedHPWL());
  EmitSnapshot("orientation", "After Cluster Orientation", "legalization",
               "orientation");
}

std::vector<GriddedRow*> GriddedCellWellLegalizer::CollectGriddedRows() {
  std::vector<GriddedRow*> rows;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        rows.push_back(&row);
      }
    }
  }
  return rows;
}

void GriddedCellWellLegalizer::RunGriddedDetailedPlacementStage() {
  LOG(info) << "Run gridded detailed placement\n";
  gridded_detailed_placer_.CopyPlacementContextFrom(this);
  gridded_detailed_placer_.SetRows(CollectGriddedRows());
  gridded_detailed_placer_.SetSnapshotCallback(
      [this](const std::string& id, const std::string& label,
             const std::string& subgroup, int iteration) {
        EmitSnapshot(id, label, "detailed_placement", subgroup, iteration);
      });
  EmitSnapshot("gridded.start", "Before Gridded Detailed Placement",
               "detailed_placement", "start");
  gridded_detailed_placer_.StartPlacement();
  RecordPlacementMetric("well_legalization.local_reorder", WeightedHPWL());
  EmitSnapshot("gridded.final", "After Gridded Detailed Placement",
               "detailed_placement", "final");
}

bool GriddedCellWellLegalizer::RunMovableCellLegalizationStages() {
  bool is_success = RunComponentClusteringStage();
  RunClusterOrientationStage();
  return is_success;
}

bool GriddedCellWellLegalizer::RetryMovableCellLegalizationWithScavenging() {
  if (stripe_mode_ == int(DefaultPartitionMode::SCAVENGE)) {
    return false;
  }
  LOG(warning) << "Strict well legalization failed; retry with scavenge mode\n";
  int previous_stripe_mode = stripe_mode_;
  stripe_mode_ = int(DefaultPartitionMode::SCAVENGE);
  snapshot_attempt_ = 1;
  RestoreInitialComponentLocation();
  InitializeWellLegalizer();
  bool is_success = RunMovableCellLegalizationStages();
  stripe_mode_ = previous_stripe_mode;
  return is_success;
}

void GriddedCellWellLegalizer::RunWellTapStage() {
  if (disable_welltap_) {
    LOG(info) << "Skip inserting well tap cells\n";
  } else {
    LOG(info) << "Insert well tap cells\n";
    WellRowCompleter(ckt_ptr_, &col_list_, BuildRowCompletionConfig())
        .InsertWellTaps();
  }
  RecordPlacementMetric("well_legalization.well_tap", WeightedHPWL());
  EmitSnapshot("well_tap", "After Well Tap Insertion", "legalization",
               "well_tap");
}

void GriddedCellWellLegalizer::RunEndCapStage() {
  if (enable_end_cap_cell_) {
    LOG(info) << "Create end cap cells\n";
    WellRowCompleter(ckt_ptr_, &col_list_, BuildRowCompletionConfig())
        .InsertEndCaps();
  } else {
    LOG(info) << "Skip creating end cap cells\n";
  }
  EmitSnapshot("end_cap", "After End Cap Insertion", "legalization", "end_cap");
}

void GriddedCellWellLegalizer::RunPhysicalCompletionStages() {
  RunWellTapStage();
  RunEndCapStage();
}

void GriddedCellWellLegalizer::EmitSnapshot(const std::string& id,
                                           const std::string& label,
                                           const std::string& group,
                                           const std::string& subgroup,
                                           int iteration) {
  if (!snapshot_callback_) return;
  snapshot_callback_("attempt_" + std::to_string(snapshot_attempt_) + "." + id,
                     label, group, subgroup, iteration);
}

bool GriddedCellWellLegalizer::StartPlacement() {
  PrintStartStatement("standard cluster well legalization");

  snapshot_attempt_ = 0;
  SaveInitialComponentLocation();
  InitializeWellLegalizer();
  bool is_success = RunMovableCellLegalizationStages();
  if (!is_success) {
    is_success = RetryMovableCellLegalizationWithScavenging();
  }
  if (!is_success) {
    LOG(error) << "Skip well tap, end cap, and well shape insertion because "
                  "movable-cell well legalization failed\n";
    PrintEndStatement("Standard Cluster Well Legalization", false);
    return false;
  }
  RunPhysicalCompletionStages();

  PrintEndStatement("Standard Cluster Well Legalization", is_success);

  return is_success;
}

void GriddedCellWellLegalizer::ReportEffectiveSpaceUtilization() {
  int total_standard_component_area = 0;
  int max_n_height = 0;
  int max_p_height = 0;
  for (auto& component : ckt_ptr_->design().Components()) {
    Macro* macro = component.MacroPtr();
    if (macro == ckt_ptr_->tech().IoDummyMacroPtr()) continue;
    if (macro->FirstNwellHeight() > max_n_height) {
      max_n_height = macro->FirstNwellHeight();
    }
    if (macro->FirstPwellHeight() > max_p_height) {
      max_p_height = macro->FirstPwellHeight();
    }
  }
  if (well_tap_macro_->FirstNwellHeight() > max_n_height) {
    max_n_height = well_tap_macro_->FirstNwellHeight();
  }
  if (well_tap_macro_->FirstPwellHeight() > max_p_height) {
    max_p_height = well_tap_macro_->FirstPwellHeight();
  }
  int max_height = max_n_height + max_p_height;

  int total_effective_component_area = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& cluster : stripe.gridded_rows_) {
        int eff_height = cluster.Height();
        int tot_cell_width = 0;
        for (auto& component_ptr : cluster.Components()) {
          tot_cell_width += component_ptr->Width();
        }
        total_effective_component_area += tot_cell_width * eff_height;
        total_standard_component_area += tot_cell_width * max_height;
      }
    }
  }
  double factor = ckt_ptr_->GridValueX() * ckt_ptr_->GridValueY();
  LOG(info) << "Total placement area: "
            << (RegionWidth() * RegionHeight()) * factor << " um^2\n";
  LOG(info) << "Total component area: "
            << ckt_ptr_->TotalComponentArea() * factor << " ("
            << ckt_ptr_->TotalComponentArea() / (double)RegionWidth() /
                   (double)RegionHeight()
            << ") um^2\n";
  LOG(info) << "Total effective component area: "
            << total_effective_component_area * factor << " ("
            << total_effective_component_area / (double)RegionWidth() /
                   (double)RegionHeight()
            << ") um^2\n";
  LOG(info) << "Total standard component area (lower bound):"
            << total_standard_component_area * factor << " ("
            << total_standard_component_area / (double)RegionWidth() /
                   (double)RegionHeight()
            << ") um^2\n";
}

void GriddedCellWellLegalizer::GenMatlabClusterTable(
    std::string const& name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  ckt_ptr_->GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

void GriddedCellWellLegalizer::GenMATLABWellTable(
    std::string const& name_of_file, int well_emit_mode) {
  ckt_ptr_->GenMATLABWellTable(name_of_file, false);

  GenMATLABWellFillingTable(name_of_file, col_list_, RegionBottom(),
                            RegionTop(), well_emit_mode);

  GenPPNP(name_of_file);
}

void GriddedCellWellLegalizer::GenPPNP(const std::string& name_of_file) {
  std::string np_file = name_of_file + "_np.txt";
  std::ofstream ostnp(np_file.c_str());
  DaliExpects(ostnp.is_open(), "Cannot open output file: " + np_file);

  std::string pp_file = name_of_file + "_pp.txt";
  std::ofstream ostpp(pp_file.c_str());
  DaliExpects(ostpp.is_open(), "Cannot open output file: " + pp_file);

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto& cluster : stripe.gridded_rows_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = stripe.is_first_row_orient_N_;
      int active_lx = LeftTapUx(stripe);
      int active_ux = RightTapLx(stripe);
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (uy > ly && active_ux > active_lx) {
          if (is_p_well_rect) {
            ostnp << active_lx << "\t" << active_ux << "\t" << active_ux << "\t"
                  << active_lx << "\t" << ly << "\t" << ly << "\t" << uy << "\t"
                  << uy << "\n";
          } else {
            ostpp << active_lx << "\t" << active_ux << "\t" << active_ux << "\t"
                  << active_lx << "\t" << ly << "\t" << ly << "\t" << uy << "\t"
                  << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }

      // draw NP and PP shapes from well-tap cell to well-tap cell
      std::vector<int> well_tap_top_bottom_list;
      well_tap_top_bottom_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionBottom());
      } else {
        well_tap_top_bottom_list.push_back(RegionTop());
      }
      for (auto& cluster : stripe.gridded_rows_) {
        Component* well_tap = cluster.WellTapCell();
        DaliExpects(well_tap != nullptr,
                    "Cannot emit P+/N+ tap regions without a well tap cell");
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(well_tap->LLY());
          well_tap_top_bottom_list.push_back(well_tap->URY());
        } else {
          well_tap_top_bottom_list.push_back(well_tap->URY());
          well_tap_top_bottom_list.push_back(well_tap->LLY());
        }
      }
      if (stripe.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionTop());
      } else {
        well_tap_top_bottom_list.push_back(RegionBottom());
        std::reverse(well_tap_top_bottom_list.begin(),
                     well_tap_top_bottom_list.end());
      }
      DaliExpects(well_tap_top_bottom_list.size() % 2 == 0,
                  "Impossible to get an even number of well tap cell edges");

      is_p_well_rect = stripe.is_first_row_orient_N_;
      int lx0 = LeftTapLx(stripe);
      int ux0 = LeftTapUx(stripe);
      int lx1 = RightTapLx(stripe);
      int ux1 = RightTapUx(stripe);
      rect_count = (int)well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          if (is_p_well_rect) {
            ostpp << lx0 << "\t" << ux0 << "\t" << ux0 << "\t" << lx0 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
            ostpp << lx1 << "\t" << ux1 << "\t" << ux1 << "\t" << lx1 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
          } else {
            ostnp << lx0 << "\t" << ux0 << "\t" << ux0 << "\t" << lx0 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
            ostnp << lx1 << "\t" << ux1 << "\t" << ux1 << "\t" << lx1 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ostnp.close();
  ostpp.close();
}

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
 *
 * @param enable_emitting_cluster
 * True: emit cluster file
 * Fale: do not emit this file
 * ****/
void GriddedCellWellLegalizer::EmitDEFWellFile(std::string const& name_of_file,
                                              int well_emit_mode,
                                              bool enable_emitting_cluster) {
  EmitPPNPRect(name_of_file + "ppnp.rect");
  EmitWellRect(name_of_file + "well.rect", well_emit_mode);
  if (enable_emitting_cluster) {
    EmitClusterRect(name_of_file + "_router.cluster");
  }
}

void GriddedCellWellLegalizer::EmitPPNPRect(std::string const& name_of_file) {
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(true);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .EmitImplantRectFile(name_of_file);
}

void GriddedCellWellLegalizer::ExportPpNpToPhyDB(phydb::PhyDB* phydb_ptr) {
  if (disable_welltap_) {
    LOG(info) << "Skip export Pplus/Nplus fillings to PhyDB "
                 "since well tap is disabled\n";
    return;
  }
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(true);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .ExportImplantsToPhyDB(phydb_ptr);
}

void GriddedCellWellLegalizer::EmitWellRect(std::string const& name_of_file,
                                           int well_emit_mode) {
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(false);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .EmitWellRectFile(name_of_file, well_emit_mode);
}

void GriddedCellWellLegalizer::ExportWellToPhyDB(phydb::PhyDB* phydb_ptr,
                                                int well_emit_mode) {
  if (disable_welltap_) {
    LOG(info) << "Skip export wells to PhyDB since well tap is disabled\n";
    return;
  }
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(false);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .ExportWellsToPhyDB(phydb_ptr, well_emit_mode);
}

std::vector<PlacementWellRect>
GriddedCellWellLegalizer::CollectWellVisualizationRects() {
  WellGeometryBuilder builder(col_list_, RegionBottom(), RegionTop());
  std::vector<WellGeometryRect> geometry =
      builder.Build(!disable_welltap_ && well_tap_macro_ != nullptr);

  std::vector<PlacementWellRect> result;
  result.reserve(geometry.size());
  for (const WellGeometryRect& geometry_rect : geometry) {
    PlacementWellLayer layer = PlacementWellLayer::kPwell;
    switch (geometry_rect.layer) {
      case WellGeometryLayer::kPwell:
        layer = PlacementWellLayer::kPwell;
        break;
      case WellGeometryLayer::kNwell:
        layer = PlacementWellLayer::kNwell;
        break;
      case WellGeometryLayer::kPplus:
        layer = PlacementWellLayer::kPplus;
        break;
      case WellGeometryLayer::kNplus:
        layer = PlacementWellLayer::kNplus;
        break;
    }
    const RectI& rect = geometry_rect.bounds;
    result.push_back({static_cast<float>(rect.LLX() * ckt_ptr_->GridValueX()),
                      static_cast<float>(rect.LLY() * ckt_ptr_->GridValueY()),
                      static_cast<float>(rect.URX() * ckt_ptr_->GridValueX()),
                      static_cast<float>(rect.URY() * ckt_ptr_->GridValueY()),
                      layer});
  }
  return result;
}

/****
 * Emits a rect file for power routing
 * ****/
void GriddedCellWellLegalizer::EmitClusterRect(std::string const& name_of_file) {
  LOG(info) << "Writing cluster rect file: " << name_of_file << "\n";
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = ckt_ptr_->DistanceScaleFactorX();
  double factor_y = ckt_ptr_->DistanceScaleFactorY();
  for (size_t i = 0; i < col_list_.size(); ++i) {
    std::string column_name = "column" + std::to_string(i);
    ost << "STRIP " << column_name << "\n";

    auto& col = col_list_[i];
    for (auto& stripe : col.stripe_list_) {
      ost << "  "
          << (int)(stripe.LLX() * factor_x) +
                 ckt_ptr_->design().DieAreaOffsetX()
          << "  "
          << (int)(stripe.URX() * factor_x) +
                 ckt_ptr_->design().DieAreaOffsetX()
          << "  ";
      if (stripe.is_first_row_orient_N_) {
        ost << "GND\n";
      } else {
        ost << "Vdd\n";
      }

      if (stripe.is_bottom_up_) {
        for (auto& cluster : stripe.gridded_rows_) {
          ost << "  "
              << (int)(cluster.LLY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "  "
              << (int)(cluster.URY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "\n";
        }
      } else {
        int sz = stripe.gridded_rows_.size();
        for (int j = sz - 1; j >= 0; --j) {
          auto& cluster = stripe.gridded_rows_[j];
          ost << "  "
              << (int)(cluster.LLY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "  "
              << (int)(cluster.URY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "\n";
        }
      }

      ost << "END " << column_name << "\n\n";
    }
  }
  ost.close();
}

}  // namespace dali
