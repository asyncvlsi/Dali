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
#include "std_cluster_well_legalizer.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>

#include "dali/common/helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/stripe_helper.h"

namespace dali {

StdClusterWellLegalizer::StdClusterWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_width_ = 0;
}

void StdClusterWellLegalizer::LoadConf(std::string const& config_file) {
  config_read(config_file.c_str());
  DaliExpects(false, "Not implemented");
}

void StdClusterWellLegalizer::CheckWellStatus() {
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (component.IsMovable()) {
      DaliExpects(component.MacroPtr()->HasWellInfo(),
                  "Cannot find well info for component: " << component.Name());
    }
  }
}

void StdClusterWellLegalizer::FetchNpWellParams() {
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
  }

  well_tap_p_height_ = well_tap_macro_->FirstPwellHeight();
  well_tap_n_height_ = well_tap_macro_->FirstNwellHeight();
}

void StdClusterWellLegalizer::SaveInitialComponentLocation() {
  component_init_locations_.clear();

  std::vector<Component>& component_list = ckt_ptr_->Components();
  component_init_locations_.reserve(component_list.size());

  for (auto& component : component_list) {
    component_init_locations_.emplace_back(component.LLX(), component.LLY());
  }
}

void StdClusterWellLegalizer::SetMaxRowWidth(double max_row_width_microns) {
  if (max_row_width_microns < 0) {
    max_row_width_ = -1;
    return;
  }
  DaliExpects(ckt_ptr_ != nullptr, "Circuit must be set before row width");
  max_row_width_ = std::floor(max_row_width_microns / ckt_ptr_->GridValueX());
  LOG(info) << "Max row width in grid unit : " << max_row_width_ << "\n";
}

void StdClusterWellLegalizer::InitializeWellLegalizer(int cluster_width) {
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

void StdClusterWellLegalizer::CreateClusterAndAppendSingleWellComponent(
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

  int space_for_well_tap = well_tap_count_per_cluster_ * well_tap_width_ +
                           well_tap_count_per_cluster_ * space_to_well_tap_;

  int space_for_end_cap = 0;
  if (enable_end_cap_cell_) {
    space_for_end_cap = pre_end_cap_min_width_ + post_end_cap_min_width_;
    // row height should be able to accommodate pre- and post-end cap cell
    front_row->UpdateWellHeightUpward(
        std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_),
        std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
  }

  front_row->SetUsedSize(space_for_well_tap + space_for_end_cap + width);
  // row height should be able to accommodate well tap cell
  front_row->UpdateWellHeightUpward(well_tap_p_height_, well_tap_n_height_);
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

void StdClusterWellLegalizer::AppendSingleWellComponentToFrontCluster(
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

void StdClusterWellLegalizer::AppendComponentToColBottomUp(
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

void StdClusterWellLegalizer::AppendComponentToColTopDown(
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
    front_row->SetUsedSize(width +
                           well_tap_count_per_cluster_ * well_tap_width_ +
                           well_tap_count_per_cluster_ * space_to_well_tap_);
    front_row->UpdateWellHeightDownward(well_tap_p_height_, well_tap_n_height_);
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

void StdClusterWellLegalizer::AppendComponentToColBottomUpCompact(
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
    front_cluster->SetUsedSize(
        width + well_tap_count_per_cluster_ * well_tap_width_ +
        well_tap_count_per_cluster_ * space_to_well_tap_);
    front_cluster->UpdateWellHeightUpward(well_tap_p_height_,
                                          well_tap_n_height_);
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

void StdClusterWellLegalizer::AppendComponentToColTopDownCompact(
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
    front_cluster->SetUsedSize(
        width + well_tap_count_per_cluster_ * well_tap_width_ +
        well_tap_count_per_cluster_ * space_to_well_tap_);
    front_cluster->UpdateWellHeightDownward(well_tap_p_height_,
                                            well_tap_n_height_);
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

bool StdClusterWellLegalizer::StripeLegalizationBottomUp(Stripe& stripe) {
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

bool StdClusterWellLegalizer::StripeLegalizationTopDown(Stripe& stripe) {
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

bool StdClusterWellLegalizer::StripeLegalizationBottomUpCompact(
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

bool StdClusterWellLegalizer::StripeLegalizationTopDownCompact(Stripe& stripe) {
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

bool StdClusterWellLegalizer::ComponentClustering() {
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
bool StdClusterWellLegalizer::ComponentClusteringLoose() {
  int step = 50;
  int count = 0;
  bool res = true;
  for (auto& col : col_list_) {
    bool is_success = true;
    for (auto& stripe : col.stripe_list_) {
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

  return res;
}

bool StdClusterWellLegalizer::ComponentClusteringCompact() {
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

bool StdClusterWellLegalizer::TrialClusterLegalization(Stripe& stripe) {
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

/****
 * Returns the wire-length cost of the small group from l-th element to r-th
 * element in this cluster "for each order, we keep the left and right
 * boundaries of the group and evenly distribute the cells inside the group.
 * Since we have the Single-Segment Clustering technique to take care of the
 * cell positions, we do not pay much attention to the exact positions of the
 * cells during Local Re-ordering." from "An Efficient and Effective Detailed
 * Placement Algorithm"
 * ****/
double StdClusterWellLegalizer::WireLengthCost(GriddedRow* cluster, int l,
                                               int r) {
  auto& net_list = ckt_ptr_->Nets();
  std::set<Net*> net_involved;
  for (int i = l; i <= r; ++i) {
    auto* component = cluster->Components()[i];
    for (auto& net_num : component->NetList()) {
      if (net_list[net_num].PinCnt() < 100) {
        net_involved.insert(&(net_list[net_num]));
      }
    }
  }

  double hpwl_x = 0;
  double hpwl_y = 0;
  for (auto& net : net_involved) {
    hpwl_x += net->WeightedHPWLX();
    hpwl_y += net->WeightedHPWLY();
  }

  return hpwl_x * ckt_ptr_->GridValueX() + hpwl_y * ckt_ptr_->GridValueY();
}

/****
 * Returns the best permutation in @param res
 * @param cost records the cost function associated with the best permutation
 * @param l is the left bound of the range
 * @param r is the right bound of the range
 * @param cluster points to the whole range, but we are only interested in the
 * permutation of range [l,r]
 * ****/
void StdClusterWellLegalizer::FindBestLocalOrder(
    std::vector<Component*>& res, double& cost, GriddedRow* cluster, int cur,
    int l, int r, int left_bound, int right_bound, int gap, int range) {
  // LOG(info)  <<"l : %d, r: %d\n", l, r);
  if (cur == r) {
    cluster->Components()[l]->SetLLX(left_bound);
    cluster->Components()[r]->SetURX(right_bound);

    int left_contour = left_bound + gap + cluster->Components()[l]->Width();
    for (int i = l + 1; i < r; ++i) {
      auto* component = cluster->Components()[i];
      component->SetLLX(left_contour);
      left_contour += component->Width() + gap;
    }

    double tmp_cost = WireLengthCost(cluster, l, r);
    if (tmp_cost < cost) {
      cost = tmp_cost;
      for (int j = 0; j < range; ++j) {
        res[j] = cluster->Components()[l + j];
      }
    }
  } else {
    // Permutations made
    auto& component_list = cluster->Components();
    for (int i = cur; i <= r; ++i) {
      // Swapping done
      std::swap(component_list[cur], component_list[i]);

      // Recursion called
      FindBestLocalOrder(res, cost, cluster, cur + 1, l, r, left_bound,
                         right_bound, gap, range);

      // backtrack
      std::swap(component_list[cur], component_list[i]);
    }
  }
}

void StdClusterWellLegalizer::LocalReorderInCluster(GriddedRow* cluster,
                                                    int range) {
  /****
   * Enumerate all local permutations, @param range determines how big the local
   * range is
   * ****/

  assert(range > 0);

  int sz = cluster->Components().size();
  if (sz < 3) return;

  std::sort(
      cluster->Components().begin(), cluster->Components().end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->LLX() < component_ptr1->LLX();
      });

  int last_segment = sz - range;
  std::vector<Component*> res_local_order(range, nullptr);
  for (int l = 0; l <= last_segment; ++l) {
    int total_component_width = 0;
    for (int j = 0; j < range; ++j) {
      res_local_order[j] = cluster->Components()[l + j];
      total_component_width += res_local_order[j]->Width();
    }
    int r = l + range - 1;
    double best_cost = DBL_MAX;
    int left_bound = (int)cluster->Components()[l]->LLX();
    int right_bound = (int)cluster->Components()[r]->URX();
    int gap = (right_bound - left_bound - total_component_width) / (r - l);

    FindBestLocalOrder(res_local_order, best_cost, cluster, l, l, r, left_bound,
                       right_bound, gap, range);
    for (int j = 0; j < range; ++j) {
      cluster->Components()[l + j] = res_local_order[j];
    }

    cluster->Components()[l]->SetLLX(left_bound);
    cluster->Components()[r]->SetURX(right_bound);
    int left_contour = left_bound + cluster->Components()[l]->Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      auto* component = cluster->Components()[i];
      component->SetLLX(left_contour);
      left_contour += component->Width() + gap;
    }
  }
}

void StdClusterWellLegalizer::LocalReorderAllClusters() {
  // sort all cluster based on their lower left corners
  size_t tot_cluster_count = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  std::vector<GriddedRow*> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& cluster : stripe.gridded_rows_) {
        cluster_ptr_list[counter] = &cluster;
        ++counter;
      }
    }
  }
  std::sort(cluster_ptr_list.begin(), cluster_ptr_list.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  for (auto& cluster_ptr : cluster_ptr_list) {
    LocalReorderInCluster(cluster_ptr, 3);
  }
}

/*
void StdClusterWellLegalizer::SingleSegmentClusteringOptimization() {
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

void StdClusterWellLegalizer::UpdateClusterOrient() {
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

void StdClusterWellLegalizer::InsertWellTap() {
  ckt_ptr_->design().WellTapComponentCollection().Clear();
  size_t tot_cluster_count = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  ckt_ptr_->design().WellTapComponentCollection().Reserve(tot_cluster_count *
                                                          2);

  int counter = 0;
  int total_well_tap_count = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        int well_tap_count = 2;
        total_well_tap_count += well_tap_count;
        int step = row.Width();
        int well_tap_loc = row.LLX() - well_tap_macro_->Width() / 2;
        for (int i = 0; i < well_tap_count; ++i) {
          std::string component_name =
              "__well_tap__" + std::to_string(counter++);
          auto [tap_cell, tap_cell_id] =
              ckt_ptr_->design().WellTapComponentCollection().CreateWithId(
                  component_name);
          tap_cell.SetPlacementStatus(PLACED);
          tap_cell.SetMacro(well_tap_macro_);
          tap_cell.SetId(static_cast<int>(tap_cell_id));
          row.InsertWellTapCell(tap_cell, well_tap_loc);
          well_tap_loc += step;
        }
        row.LegalizeLooseX(space_to_well_tap_);
      }
    }
  }

  ckt_ptr_->design().WellTapComponentCollection().Freeze();
  LOG(info) << "Insertion complete: " << total_well_tap_count
            << " well tap cell created\n";
}

/**
 * @brief Creates end-cap cell types for each unique (NHeight, PHeight)
 * combination found in the gridded rows of stripes in columns.
 *
 * This function iterates through the columns, stripes, and gridded rows in the
 * column list. For each unique combination of NHeight and PHeight in a row, it
 * creates both pre and post end-cap cell types. These cell types are then
 * stored in the `pre_end_cap_cell_np_heights_to_type` map to avoid duplication.
 */
void StdClusterWellLegalizer::CreateEndCapMacros() {
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        std::tuple<int, int> np_height = {row.NHeight(), row.PHeight()};

        // Check if the end-cap cell type for this height combination already
        // exists
        if (pre_end_cap_cell_np_heights_to_type_id.find(np_height) ==
            pre_end_cap_cell_np_heights_to_type_id.end()) {
          // Create and register the pre end-cap cell type
          std::string pre_end_cap_cell_name =
              "pre_end_cap_n_height_" + std::to_string(row.NHeight()) +
              "_p_height_" + std::to_string(row.PHeight());
          int pre_end_cap_cell_macro_id = ckt_ptr_->CreateEndCapMacro(
              pre_end_cap_cell_name, ckt_ptr_->tech().PreEndCapMinWidth(),
              row.NHeight(), row.PHeight());
          pre_end_cap_cell_np_heights_to_type_id[np_height] =
              pre_end_cap_cell_macro_id;

          // Create and register the post end-cap cell type
          std::string post_end_cap_cell_name =
              "post_end_cap_n_height_" + std::to_string(row.NHeight()) +
              "_p_height_" + std::to_string(row.PHeight());
          int post_end_cap_cell_macro_id = ckt_ptr_->CreateEndCapMacro(
              post_end_cap_cell_name, ckt_ptr_->tech().PostEndCapMinWidth(),
              row.NHeight(), row.PHeight());
          post_end_cap_cell_np_heights_to_type_id[np_height] =
              post_end_cap_cell_macro_id;
        }
      }
    }
  }
  ckt_ptr_->tech().EndCapCellMacroCollection().Freeze();
}

void StdClusterWellLegalizer::InsertEndCapCells() {
  // Clear all existing instances
  ckt_ptr_->design().EndCapComponentCollection().Clear();
  size_t tot_cluster_count = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  ckt_ptr_->design().EndCapComponentCollection().Reserve(tot_cluster_count * 2);

  int row_counter = 0;
  int total_num_end_cap_cells = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        std::tuple<int, int> np_height = {row.NHeight(), row.PHeight()};

        // Create pre end cap cell
        int pre_end_cap_cell_macro_id =
            pre_end_cap_cell_np_heights_to_type_id[np_height];
        Macro* pre_end_cap_cell_macro_ptr =
            ckt_ptr_->tech().EndCapCellMacroCollection().GetInstanceById(
                pre_end_cap_cell_macro_id);
        int pre_end_cap_cell_loc =
            row.LLX() - pre_end_cap_cell_macro_ptr->Width() / 2;
        std::string pre_end_cap_cell_name =
            "__pre_end_cap_cell__" + std::to_string(row_counter);
        auto [pre_end_cap_cell, pre_end_cap_cell_id] =
            ckt_ptr_->design().EndCapComponentCollection().CreateWithId(
                pre_end_cap_cell_name);
        pre_end_cap_cell.SetPlacementStatus(PLACED);
        pre_end_cap_cell.SetMacro(pre_end_cap_cell_macro_ptr);
        pre_end_cap_cell.SetId(static_cast<int>(pre_end_cap_cell_id));
        row.InsertWellTapCell(pre_end_cap_cell, pre_end_cap_cell_loc);

        // Create post end cap cell
        int post_end_cap_cell_macro_id =
            post_end_cap_cell_np_heights_to_type_id[np_height];
        Macro* post_end_cap_cell_macro_ptr =
            ckt_ptr_->tech().EndCapCellMacroCollection().GetInstanceById(
                post_end_cap_cell_macro_id);
        int post_end_cap_cell_loc =
            row.URX() + post_end_cap_cell_macro_ptr->Width() / 2;
        std::string post_end_cap_cell_name =
            "__post_end_cap_cell__" + std::to_string(row_counter);
        auto [post_end_cap_cell, post_end_cap_cell_id] =
            ckt_ptr_->design().EndCapComponentCollection().CreateWithId(
                post_end_cap_cell_name);
        post_end_cap_cell.SetPlacementStatus(PLACED);
        post_end_cap_cell.SetMacro(post_end_cap_cell_macro_ptr);
        post_end_cap_cell.SetId(static_cast<int>(post_end_cap_cell_id));
        row.InsertWellTapCell(post_end_cap_cell, post_end_cap_cell_loc);

        total_num_end_cap_cells += 2;
        row_counter += 1;

        row.LegalizeLooseX(space_to_well_tap_);
      }
    }
  }

  ckt_ptr_->design().EndCapComponentCollection().Freeze();
  LOG(info) << "Insertion complete: " << total_num_end_cap_cells
            << " pre- and post- end cap cells created\n";
}

void StdClusterWellLegalizer::ClearCachedData() {
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

bool StdClusterWellLegalizer::WellLegalize() {
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

bool StdClusterWellLegalizer::RunComponentClusteringStage() {
  LOG(info) << "Form component clustering\n";
  bool is_success = ComponentClusteringLoose();
  ReportHPWL();
  RecordPlacementMetric("well_legalization.component_clustering",
                        WeightedHPWL());
  return is_success;
}

void StdClusterWellLegalizer::RunClusterOrientationStage() {
  if (disable_cell_flip_) {
    LOG(info) << "Skip flipping cluster orientation\n";
    return;
  }
  LOG(info) << "Flip cluster orientation\n";
  UpdateClusterOrient();
  ReportHPWL();
  RecordPlacementMetric("well_legalization.orientation", WeightedHPWL());
}

void StdClusterWellLegalizer::RunLocalReorderingStage() {
  LOG(info) << "Perform local reordering\n";
  for (int i = 0; i < 6; ++i) {
    LOG(info) << "reorder iteration: " << i << "\n";
    LocalReorderAllClusters();
    ReportHPWL();
    RecordPlacementMetric("well_legalization.local_reorder", WeightedHPWL());
  }
}

bool StdClusterWellLegalizer::RunMovableCellLegalizationStages() {
  bool is_success = RunComponentClusteringStage();
  RunClusterOrientationStage();
  RunLocalReorderingStage();
  return is_success;
}

void StdClusterWellLegalizer::RunWellTapStage() {
  if (disable_welltap_) {
    LOG(info) << "Skip inserting well tap cells\n";
  } else {
    LOG(info) << "Insert well tap cells\n";
    InsertWellTap();
  }
  RecordPlacementMetric("well_legalization.well_tap", WeightedHPWL());
}

void StdClusterWellLegalizer::RunEndCapStage() {
  if (enable_end_cap_cell_) {
    LOG(info) << "Create end cap cells\n";
    CreateEndCapMacros();
    InsertEndCapCells();
  } else {
    LOG(info) << "Skip creating end cap cells\n";
  }
}

void StdClusterWellLegalizer::RunPhysicalCompletionStages() {
  RunWellTapStage();
  RunEndCapStage();
}

bool StdClusterWellLegalizer::StartPlacement() {
  PrintStartStatement("standard cluster well legalization");

  InitializeWellLegalizer();
  bool is_success = RunMovableCellLegalizationStages();
  RunPhysicalCompletionStages();

  PrintEndStatement("Standard Cluster Well Legalization", is_success);

  return is_success;
}

void StdClusterWellLegalizer::ReportEffectiveSpaceUtilization() {
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

void StdClusterWellLegalizer::GenMatlabClusterTable(
    std::string const& name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  ckt_ptr_->GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

void StdClusterWellLegalizer::GenMATLABWellTable(
    std::string const& name_of_file, int well_emit_mode) {
  ckt_ptr_->GenMATLABWellTable(name_of_file, false);

  GenMATLABWellFillingTable(name_of_file, col_list_, RegionBottom(),
                            RegionTop(), well_emit_mode);

  GenPPNP(name_of_file);
}

void StdClusterWellLegalizer::GenPPNP(const std::string& name_of_file) {
  std::string np_file = name_of_file + "_np.txt";
  std::ofstream ostnp(np_file.c_str());
  DaliExpects(ostnp.is_open(), "Cannot open output file: " + np_file);

  std::string pp_file = name_of_file + "_pp.txt";
  std::ofstream ostpp(pp_file.c_str());
  DaliExpects(ostpp.is_open(), "Cannot open output file: " + pp_file);

  int adjust_width = well_tap_macro_->Width();

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
      int lx = stripe.LLX();
      int ux = stripe.URX();
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          ostnp << lx + adjust_width << "\t" << ux - adjust_width << "\t"
                << ux - adjust_width << "\t" << lx + adjust_width << "\t" << ly
                << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
        } else {
          ostpp << lx + adjust_width << "\t" << ux - adjust_width << "\t"
                << ux - adjust_width << "\t" << lx + adjust_width << "\t" << ly
                << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
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
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
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
      int lx0 = stripe.LLX();
      int ux0 = lx + adjust_width;
      int ux1 = stripe.URX();
      int lx1 = ux1 - adjust_width;
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
void StdClusterWellLegalizer::EmitDEFWellFile(std::string const& name_of_file,
                                              int well_emit_mode,
                                              bool enable_emitting_cluster) {
  EmitPPNPRect(name_of_file + "ppnp.rect");
  EmitWellRect(name_of_file + "well.rect", well_emit_mode);
  if (enable_emitting_cluster) {
    EmitClusterRect(name_of_file + "_router.cluster");
  }
}

void StdClusterWellLegalizer::EmitPPNPRect(std::string const& name_of_file) {
  // emit rect file
  std::string NP_name = "nplus";
  std::string PP_name = "pplus";

  LOG(info) << "Writing PP and NP rect file: " << name_of_file << "\n";

  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = ckt_ptr_->DistanceScaleFactorX();
  double factor_y = ckt_ptr_->DistanceScaleFactorY();

  ost << "bbox " << ckt_ptr_->LocDali2PhydbX(RegionLeft()) << " "
      << ckt_ptr_->LocDali2PhydbY(RegionBottom()) << " "
      << ckt_ptr_->LocDali2PhydbX(RegionRight()) << " "
      << ckt_ptr_->LocDali2PhydbY(RegionTop()) << "\n";

  int adjust_width = well_tap_macro_->Width();

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
      int lx = stripe.LLX();
      int ux = stripe.URX();
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          ost << "rect # " << NP_name << " ";
        } else {
          ost << "rect # " << PP_name << " ";
        }
        ost << (lx + adjust_width) * factor_x +
                   ckt_ptr_->design().DieAreaOffsetX()
            << "\t" << ly * factor_y + ckt_ptr_->design().DieAreaOffsetY()
            << "\t"
            << (ux - adjust_width) * factor_x +
                   ckt_ptr_->design().DieAreaOffsetX()
            << "\t" << uy * factor_y + ckt_ptr_->design().DieAreaOffsetY()
            << "\n";

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
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
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
      int lx0 = stripe.LLX();
      int ux0 = lx + adjust_width;
      int ux1 = stripe.URX();
      int lx1 = ux1 - adjust_width;
      rect_count = (int)well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          if (!is_p_well_rect) {
            ost << "rect # " << NP_name << " ";
          } else {
            ost << "rect # " << PP_name << " ";
          }
          ost << lx0 * factor_x + ckt_ptr_->design().DieAreaOffsetX() << "\t"
              << ly * factor_y + ckt_ptr_->design().DieAreaOffsetY() << "\t"
              << ux0 * factor_x + ckt_ptr_->design().DieAreaOffsetX() << "\t"
              << uy * factor_y + ckt_ptr_->design().DieAreaOffsetY() << "\n";
          if (!is_p_well_rect) {
            ost << "rect # " << NP_name << " ";
          } else {
            ost << "rect # " << PP_name << " ";
          }
          ost << lx1 * factor_x + ckt_ptr_->design().DieAreaOffsetX() << "\t"
              << ly * factor_y + ckt_ptr_->design().DieAreaOffsetY() << "\t"
              << ux1 * factor_x + ckt_ptr_->design().DieAreaOffsetX() << "\t"
              << uy * factor_y + ckt_ptr_->design().DieAreaOffsetY() << "\n";
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ost.close();
}

void StdClusterWellLegalizer::ExportPpNpToPhyDB(phydb::PhyDB* phydb_ptr) {
  if (disable_welltap_) {
    LOG(info) << "Skip export Pplus/Nplus fillings to PhyDB "
                 "since well tap is disabled\n";
    return;
  }
  DaliExpects(phydb_ptr != nullptr, "Cannot export plus layer to a nullptr");
  LOG(info) << "Export Pplus/Nplus fillings to PhyDB\n";
  std::string NP_name = "nplus";
  std::string PP_name = "pplus";

  double factor_x =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueX();
  double factor_y =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueY();

  int bbox_llx = ckt_ptr_->LocDali2PhydbX(RegionLeft());
  int bbox_lly = ckt_ptr_->LocDali2PhydbY(RegionBottom());
  int bbox_urx = ckt_ptr_->LocDali2PhydbX(RegionRight());
  int bbox_ury = ckt_ptr_->LocDali2PhydbY(RegionTop());

  auto* phydb_layout_container = phydb_ptr->CreatePpNpMacroAndComponent(
      bbox_llx, bbox_lly, bbox_urx, bbox_ury);

  int adjust_width = well_tap_macro_->Width();

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
      int lx = stripe.LLX();
      int ux = stripe.URX();
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        std::string signal_name("#");
        std::string layer_name;
        if (is_p_well_rect) {
          layer_name = NP_name;
        } else {
          layer_name = PP_name;
        }
        int rect_llx = (int)((lx + adjust_width) * factor_x) +
                       ckt_ptr_->design().DieAreaOffsetX();
        int rect_lly =
            (int)(ly * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
        int rect_urx = (int)((ux - adjust_width) * factor_x) +
                       ckt_ptr_->design().DieAreaOffsetX();
        int rect_ury =
            (int)(uy * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
        phydb_layout_container->AddRectSignalLayer(
            signal_name, layer_name, rect_llx, rect_lly, rect_urx, rect_ury);
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
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Components()[0]->LLY());
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
      int lx0 = stripe.LLX();
      int ux0 = lx + adjust_width;
      int ux1 = stripe.URX();
      int lx1 = ux1 - adjust_width;
      rect_count = (int)well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          std::string signal_name("#");
          std::string layer_name;
          if (!is_p_well_rect) {
            layer_name = NP_name;
          } else {
            layer_name = PP_name;
          }
          int rect_llx =
              (int)(lx0 * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
          int rect_lly =
              (int)(ly * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
          int rect_urx =
              (int)(ux0 * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
          int rect_ury =
              (int)(uy * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
          phydb_layout_container->AddRectSignalLayer(
              signal_name, layer_name, rect_llx, rect_lly, rect_urx, rect_ury);

          if (!is_p_well_rect) {
            layer_name = NP_name;
          } else {
            layer_name = PP_name;
          }
          rect_llx =
              (int)(lx1 * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
          rect_lly = (int)(ly * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
          rect_urx =
              (int)(ux1 * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
          rect_ury = (int)(uy * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
          phydb_layout_container->AddRectSignalLayer(
              signal_name, layer_name, rect_llx, rect_lly, rect_urx, rect_ury);
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
}

void StdClusterWellLegalizer::EmitWellRect(std::string const& name_of_file,
                                           int well_emit_mode) {
  // emit rect file
  switch (well_emit_mode) {
    case 0: {
      LOG(info) << "Writing N/P-well rect file: " << name_of_file << "\n";
      break;
    }
    case 1: {
      LOG(info) << "Writing N-well rect file: " << name_of_file << "\n";
      break;
    }
    case 2: {
      LOG(info) << "Writing P-well rect file: " << name_of_file << "\n";
      break;
    }
    default: {
      DaliExpects(false, "Invalid value for well_emit_mode");
    }
  }

  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueX();
  double factor_y =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueY();

  ost << "bbox "
      << (int)(RegionLeft() * factor_x) + ckt_ptr_->design().DieAreaOffsetX()
      << " "
      << (int)(RegionBottom() * factor_y) + ckt_ptr_->design().DieAreaOffsetY()
      << " "
      << (int)(RegionRight() * factor_x) + ckt_ptr_->design().DieAreaOffsetX()
      << " "
      << (int)(RegionTop() * factor_y) + ckt_ptr_->design().DieAreaOffsetY()
      << "\n";
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      std::vector<int> pn_edge_list;
      if (stripe.is_bottom_up_) {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
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
      int lx = stripe.LLX();
      int ux = stripe.URX();
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
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
        ost << (int)(lx * factor_x) + ckt_ptr_->design().DieAreaOffsetX() << " "
            << (int)(ly * factor_y) + ckt_ptr_->design().DieAreaOffsetY() << " "
            << (int)(ux * factor_x) + ckt_ptr_->design().DieAreaOffsetX() << " "
            << (int)(uy * factor_y) + ckt_ptr_->design().DieAreaOffsetY()
            << "\n";
      }
    }
  }
  ost.close();
}

void StdClusterWellLegalizer::ExportWellToPhyDB(phydb::PhyDB* phydb_ptr,
                                                int well_emit_mode) {
  if (disable_welltap_) {
    LOG(info) << "Skip export wells to PhyDB since well tap is disabled\n";
    return;
  }
  switch (well_emit_mode) {
    case 0:
      LOG(info) << "Export N/P wells to PhyDB\n";
      break;
    case 1:
      LOG(info) << "Export N wells to PhyDB\n";
      break;
    case 2:
      LOG(info) << "Export P wells tp PhyDB\n";
      break;
    default:
      DaliExpects(false,
                  "Invalid value for well_emit_mode in "
                  "StdClusterWellLegalizer::EmitDEFWellFile()");
  }
  double factor_x =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueX();
  double factor_y =
      ckt_ptr_->design().DistanceMicrons() * ckt_ptr_->GridValueY();

  int bbox_llx =
      (int)(RegionLeft() * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
  int bbox_lly =
      (int)(RegionBottom() * factor_y) + ckt_ptr_->design().DieAreaOffsetY();
  int bbox_urx =
      (int)(RegionRight() * factor_x) + ckt_ptr_->design().DieAreaOffsetX();
  int bbox_ury =
      (int)(RegionTop() * factor_y) + ckt_ptr_->design().DieAreaOffsetY();

  auto* phydb_layout_container = phydb_ptr->CreateWellLayerMacroAndComponent(
      bbox_llx, bbox_lly, bbox_urx, bbox_ury);

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      std::vector<RectI> n_rects;
      std::vector<RectI> p_rects;
      CollectWellFillingRects(stripe, RegionBottom(), RegionTop(), n_rects,
                              p_rects);
      if (well_emit_mode != 1) {
        std::string signal_name = "GND";
        std::string layer_name = "pwell";
        for (auto& rect : p_rects) {
          int rect_llx = ckt_ptr_->LocDali2PhydbX(rect.LLX());
          int rect_lly = ckt_ptr_->LocDali2PhydbY(rect.LLY());
          int rect_urx = ckt_ptr_->LocDali2PhydbX(rect.URX());
          int rect_ury = ckt_ptr_->LocDali2PhydbY(rect.URY());
          phydb_layout_container->AddRectSignalLayer(
              signal_name, layer_name, rect_llx, rect_lly, rect_urx, rect_ury);
        }
      }
      if (well_emit_mode != 2) {
        std::string signal_name = "Vdd";
        std::string layer_name = "nwell";
        for (auto& rect : p_rects) {
          int rect_llx = ckt_ptr_->LocDali2PhydbX(rect.LLX());
          int rect_lly = ckt_ptr_->LocDali2PhydbY(rect.LLY());
          int rect_urx = ckt_ptr_->LocDali2PhydbX(rect.URX());
          int rect_ury = ckt_ptr_->LocDali2PhydbY(rect.URY());
          phydb_layout_container->AddRectSignalLayer(
              signal_name, layer_name, rect_llx, rect_lly, rect_urx, rect_ury);
        }
      }
    }
  }
}

/****
 * Emits a rect file for power routing
 * ****/
void StdClusterWellLegalizer::EmitClusterRect(std::string const& name_of_file) {
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
