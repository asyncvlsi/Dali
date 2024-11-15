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
#include "stdclusterwelllegalizer.h"

#include <algorithm>
#include <set>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/stripehelper.h"

namespace dali {

StdClusterWellLegalizer::StdClusterWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_cell_width_ = 0;
}

void StdClusterWellLegalizer::LoadConf(std::string const &config_file) {
  config_read(config_file.c_str());
  DaliExpects(false, "Not implemented");
}

void StdClusterWellLegalizer::CheckWellStatus() {
  auto &blocks = ckt_ptr_->Blocks();
  for (Block &blk : blocks) {
    if (blk.IsMovable()) {
      DaliExpects(blk.TypePtr()->HasWellInfo(),
                  "Cannot find well info for cell: " << blk.Name());
    }
  }
}

void StdClusterWellLegalizer::FetchNpWellParams() {
  Tech &tech = ckt_ptr_->tech();
  WellLayer &n_well_layer = tech.NwellLayer();
  double grid_value_x = ckt_ptr_->GridValueX();
  int same_well_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_well_spacing =
      std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ =
      (int)std::floor(n_well_layer.MaxPlugDist() / grid_value_x);
  DaliExpects(!ckt_ptr_->tech().WellTapCellIds().empty(),
              "Cannot find the definition of well tap cell, well legalization "
              "cannot proceed\n");
  int well_tap_cell_type_id = ckt_ptr_->tech().WellTapCellIds()[0];
  well_tap_cell_ptr_ = &(ckt_ptr_->tech().BlockTypes()[well_tap_cell_type_id]);
  well_tap_cell_width_ = well_tap_cell_ptr_->Width();

  BOOST_LOG_TRIVIAL(info) << "  Well max plug distance: "
                          << n_well_layer.MaxPlugDist() << "um, "
                          << max_unplug_length_ << " \n";
  BOOST_LOG_TRIVIAL(info) << "  GridValueX: " << ckt_ptr_->GridValueX()
                          << " um\n";
  BOOST_LOG_TRIVIAL(info) << "  Well spacing: " << n_well_layer.Spacing()
                          << "um, " << well_spacing_ << "\n";
  BOOST_LOG_TRIVIAL(info) << "  Well tap cell width: " << well_tap_cell_width_
                          << "\n";

  if (enable_end_cap_cell_) {
    pre_end_cap_min_width_ = ckt_ptr_->tech().PreEndCapMinWidth();
    BOOST_LOG_TRIVIAL(info)
        << "  pre_end_cap_min_width: " << pre_end_cap_min_width_ << "\n";

    pre_end_cap_min_p_height_ = ckt_ptr_->tech().PreEndCapMinPHeight();
    BOOST_LOG_TRIVIAL(info)
        << "  pre_end_cap_min_p_height: " << pre_end_cap_min_p_height_ << "\n";

    pre_end_cap_min_n_height_ = ckt_ptr_->tech().PreEndCapMinNHeight();
    BOOST_LOG_TRIVIAL(info)
        << "  pre_end_cap_min_n_height: " << pre_end_cap_min_n_height_ << "\n";

    post_end_cap_min_width_ = ckt_ptr_->tech().PostEndCapMinWidth();
    BOOST_LOG_TRIVIAL(info)
        << "  post_end_cap_min_width: " << post_end_cap_min_width_ << "\n";

    post_end_cap_min_p_height_ = ckt_ptr_->tech().PostEndCapMinPHeight();
    BOOST_LOG_TRIVIAL(info)
        << "  post_end_cap_min_p_height: " << post_end_cap_min_p_height_
        << "\n";

    post_end_cap_min_n_height_ = ckt_ptr_->tech().PostEndCapMinNHeight();
    BOOST_LOG_TRIVIAL(info)
        << "  post_end_cap_min_n_height: " << post_end_cap_min_n_height_
        << "\n";
  }

  tap_cell_p_height_ = well_tap_cell_ptr_->Pheight();
  tap_cell_n_height_ = well_tap_cell_ptr_->Nheight();
}

void StdClusterWellLegalizer::SaveInitialBlockLocation() {
  block_init_locations_.clear();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  block_init_locations_.reserve(block_list.size());

  for (auto &block : block_list) {
    block_init_locations_.emplace_back(block.LLX(), block.LLY());
  }
}

void StdClusterWellLegalizer::InitializeWellLegalizer(int cluster_width) {
  if (disable_welltap_) {
    num_of_tap_cell_ = 0;
    BOOST_LOG_TRIVIAL(info)
        << "set number of tap cells to 0, since well tap is disabled\n";
  }

  CheckWellStatus();

  // fetch parameters related to N/P-well
  FetchNpWellParams();

  space_partitioner_.SetInputCircuit(ckt_ptr_);
  space_partitioner_.SetOutput(&col_list_);
  if (disable_welltap_) {
    space_partitioner_.SetReservedSpaceToBoundaries(0, 0, 0, 0);
  } else {
    space_partitioner_.SetReservedSpaceToBoundaries(well_spacing_,
                                                    well_spacing_, 1, 1);
  }
  space_partitioner_.SetPartitionMode(stripe_mode_);
  space_partitioner_.SetMaxRowWidth(cluster_width);
  space_partitioner_.StartPartitioning();

  index_loc_list_.resize(ckt_ptr_->Blocks().size());
}

void StdClusterWellLegalizer::CreateClusterAndAppendSingleWellBlock(
    Stripe &stripe, Block &blk) {
  stripe.gridded_rows_.emplace_back();
  GriddedRow *front_row = &(stripe.gridded_rows_.back());
  front_row->Blocks().reserve(stripe.max_blk_capacity_per_cluster_);
  front_row->AddBlock(&blk);

  int width = blk.Width();
  int init_y = (int)std::round(blk.LLY());
  init_y = std::max(init_y, stripe.contour_);

  int p_well_height = blk.TypePtr()->Pheight();
  int n_well_height = blk.TypePtr()->Nheight();

  int space_for_well_tap = num_of_tap_cell_ * well_tap_cell_width_ +
                           num_of_tap_cell_ * space_to_well_tap_;

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
  front_row->UpdateWellHeightUpward(tap_cell_p_height_, tap_cell_n_height_);
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

void StdClusterWellLegalizer::AppendSingleWellBlockToFrontCluster(
    Stripe &stripe, Block &blk) {
  int width = blk.Width();
  int p_well_height = blk.TypePtr()->Pheight();
  int n_well_height = blk.TypePtr()->Nheight();

  GriddedRow *front_row = stripe.front_row_;
  front_row->AddBlock(&blk);
  front_row->UseSpace(width);
  if (p_well_height > front_row->PHeight() ||
      n_well_height > front_row->NHeight()) {
    int old_height = front_row->Height();
    front_row->UpdateWellHeightUpward(p_well_height, n_well_height);
    stripe.used_height_ += front_row->Height() - old_height;
  }
  stripe.contour_ = front_row->URY();
}

void StdClusterWellLegalizer::AppendBlockToColBottomUp(Stripe &stripe,
                                                       Block &blk) {
  bool is_no_row_in_col = (stripe.contour_ == stripe.LLY());
  bool is_new_row_needed = is_no_row_in_col;
  if (!is_new_row_needed) {
    GriddedRow *front_row = stripe.front_row_;
    bool is_not_in_top_row = stripe.contour_ <= blk.LLY();
    bool is_top_row_full = front_row->UsedSize() + blk.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  if (is_new_row_needed) {
    CreateClusterAndAppendSingleWellBlock(stripe, blk);
  } else {
    AppendSingleWellBlockToFrontCluster(stripe, blk);
  }
}

void StdClusterWellLegalizer::AppendBlockToColTopDown(Stripe &stripe,
                                                      Block &blk) {
  bool is_no_row = stripe.gridded_rows_.empty();
  bool is_new_row_needed = is_no_row;
  if (!is_new_row_needed) {
    bool is_not_in_top_row = stripe.contour_ >= blk.URY();
    bool is_top_row_full =
        stripe.front_row_->UsedSize() + blk.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  int width = blk.Width();
  int init_y = (int)std::round(blk.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow *front_row;
  int p_well_height = blk.TypePtr()->Pheight();
  int n_well_height = blk.TypePtr()->Nheight();
  if (is_new_row_needed) {
    stripe.gridded_rows_.emplace_back();
    front_row = &(stripe.gridded_rows_.back());
    front_row->Blocks().reserve(stripe.max_blk_capacity_per_cluster_);
    front_row->AddBlock(&blk);
    // int num_of_tap_cell = (int) std::ceil(stripe.Width() /
    // max_unplug_length_);
    int num_of_tap_cell = 2;
    front_row->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ +
                           num_of_tap_cell * space_to_well_tap_);
    front_row->UpdateWellHeightDownward(tap_cell_p_height_, tap_cell_n_height_);
    front_row->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_row->SetURY(init_y);
    front_row->SetLLX(stripe.LLX());
    front_row->SetWidth(stripe.Width());

    stripe.front_row_ = front_row;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_row->Height();
  } else {
    front_row = stripe.front_row_;
    front_row->AddBlock(&blk);
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

void StdClusterWellLegalizer::AppendBlockToColBottomUpCompact(Stripe &stripe,
                                                              Block &blk) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.LLY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + blk.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int)std::round(blk.LLY());
  init_y = std::max(init_y, stripe.contour_);

  GriddedRow *front_cluster;
  int p_well_height = blk.TypePtr()->Pheight();
  int n_well_height = blk.TypePtr()->Nheight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Blocks().reserve(stripe.max_blk_capacity_per_cluster_);
    front_cluster->AddBlock(&blk);
    // int num_of_tap_cell = (int) std::ceil(stripe.Width() /
    // max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ +
                               num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightUpward(tap_cell_p_height_,
                                          tap_cell_n_height_);
    front_cluster->SetLLY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());
    front_cluster->UpdateWellHeightUpward(p_well_height, n_well_height);

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddBlock(&blk);
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

void StdClusterWellLegalizer::AppendBlockToColTopDownCompact(Stripe &stripe,
                                                             Block &blk) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.URY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + blk.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = blk.Width();
  int init_y = (int)std::round(blk.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow *front_cluster;
  int p_well_height = blk.TypePtr()->Pheight();
  int n_well_height = blk.TypePtr()->Nheight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Blocks().reserve(stripe.max_blk_capacity_per_cluster_);
    front_cluster->AddBlock(&blk);
    // int num_of_tap_cell = (int) std::ceil(stripe.Width() /
    // max_unplug_length_);
    int num_of_tap_cell = 2;
    front_cluster->SetUsedSize(width + num_of_tap_cell * well_tap_cell_width_ +
                               num_of_tap_cell * space_to_well_tap_);
    front_cluster->UpdateWellHeightDownward(tap_cell_p_height_,
                                            tap_cell_n_height_);
    front_cluster->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_cluster->SetURY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddBlock(&blk);
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

bool StdClusterWellLegalizer::StripeLegalizationBottomUp(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.LLY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.blk_ptrs_vec_.begin(), stripe.blk_ptrs_vec_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr : stripe.blk_ptrs_vec_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColBottomUp(stripe, *blk_ptr);
  }

  for (auto &gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateBlockLocY();
  }

  return stripe.HasNoRowsSpillingOut();
}

bool StdClusterWellLegalizer::StripeLegalizationTopDown(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.blk_ptrs_vec_.begin(), stripe.blk_ptrs_vec_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr : stripe.blk_ptrs_vec_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColTopDown(stripe, *blk_ptr);
  }

  for (auto &gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateBlockLocY();
  }

  /*BOOST_LOG_TRIVIAL(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    BOOST_LOG_TRIVIAL(info)   << "success\n";
  } else {
    BOOST_LOG_TRIVIAL(info)   << "fail\n";
  }*/

  return stripe.HasNoRowsSpillingOut();
}

bool StdClusterWellLegalizer::StripeLegalizationBottomUpCompact(
    Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = RegionBottom();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.blk_ptrs_vec_.begin(), stripe.blk_ptrs_vec_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr : stripe.blk_ptrs_vec_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColBottomUpCompact(stripe, *blk_ptr);
  }

  for (auto &cluster : stripe.gridded_rows_) {
    cluster.UpdateBlockLocY();
  }

  return stripe.contour_ <= RegionTop();
}

bool StdClusterWellLegalizer::StripeLegalizationTopDownCompact(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = false;

  std::sort(stripe.blk_ptrs_vec_.begin(), stripe.blk_ptrs_vec_.end(),
            [](const Block *lhs, const Block *rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto &blk_ptr : stripe.blk_ptrs_vec_) {
    if (blk_ptr->IsFixed()) continue;
    AppendBlockToColTopDownCompact(stripe, *blk_ptr);
  }

  for (auto &cluster : stripe.gridded_rows_) {
    cluster.UpdateBlockLocY();
  }

  /*BOOST_LOG_TRIVIAL(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    BOOST_LOG_TRIVIAL(info)   << "success\n";
  } else {
    BOOST_LOG_TRIVIAL(info)   << "fail\n";
  }*/

  return stripe.contour_ >= RegionBottom();
}

bool StdClusterWellLegalizer::BlockClustering() {
  /****
   * Clustering blocks in each stripe
   * After clustering, close pack clusters from bottom to top
   ****/
  bool res = true;
  for (auto &col : col_list_) {
    bool is_success = true;
    for (auto &stripe : col.stripe_list_) {
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
        for (auto &cluster : stripe.gridded_rows_) {
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateBlockLocY();
          cluster.LegalizeCompactX();
        }
      } else {
        int sz = static_cast<int>(stripe.gridded_rows_.size());
        for (int i = sz - 1; i >= 0; --i) {
          auto &cluster = stripe.gridded_rows_[i];
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateBlockLocY();
          cluster.LegalizeCompactX();
        }
      }
    }
  }
  return res;
}

/****
 * Clustering blocks in each stripe
 * After clustering, leave clusters as they are
 * ****/
bool StdClusterWellLegalizer::BlockClusteringLoose() {
  int step = 50;
  int count = 0;
  bool res = true;
  for (auto &col : col_list_) {
    bool is_success = true;
    for (auto &stripe : col.stripe_list_) {
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
        BOOST_LOG_TRIVIAL(info)  <<"stripe legalization success, %d\n", i);
      } else {
        BOOST_LOG_TRIVIAL(info)  <<"stripe legalization fail, %d\n", i);
      }*/

      for (auto &row : stripe.gridded_rows_) {
        row.UpdateBlockLocY();
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

bool StdClusterWellLegalizer::BlockClusteringCompact() {
  /****
   * Clustering blocks in each stripe in a compact way
   * After clustering, leave clusters as they are
   * ****/

  bool res = true;
  for (auto &col : col_list_) {
    bool is_success = true;
    for (auto &stripe : col.stripe_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripeLegalizationBottomUpCompact(stripe);
        if (!is_success) {
          is_success = StripeLegalizationTopDownCompact(stripe);
        }
      }
      res = res && is_success;

      for (auto &cluster : stripe.gridded_rows_) {
        cluster.UpdateBlockLocY();
        cluster.LegalizeLooseX();
      }
    }
  }

  return res;
}

bool StdClusterWellLegalizer::TrialClusterLegalization(Stripe &stripe) {
  /****
   * Legalize the location of all clusters using extended Tetris legalization
   * algorithm in columns where usage does not exceed capacity Closely pack the
   * column from bottom to top if its usage exceeds its capacity
   * ****/

  bool res = true;

  // sort clusters in each column based on the lower left coordinate
  std::vector<GriddedRow *> cluster_list;
  cluster_list.resize(stripe.cluster_count_, nullptr);
  for (int i = 0; i < stripe.cluster_count_; ++i) {
    cluster_list[i] = &stripe.gridded_rows_[i];
  }

  // BOOST_LOG_TRIVIAL(info)   << "used height/RegionHeight(): " <<
  // col.used_height_ / (double) RegionHeight() << "\n";
  if (stripe.used_height_ <= RegionHeight()) {
    if (stripe.is_bottom_up_) {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow *lhs, const GriddedRow *rhs) {
                  return (lhs->URY() > rhs->URY());
                });
      int cluster_contour = stripe.URY();
      int res_y;
      int init_y;
      for (auto &cluster : cluster_list) {
        init_y = cluster->URY();
        res_y = std::min(cluster_contour, cluster->URY());
        cluster->SetURY(res_y);
        cluster_contour = cluster->LLY();
        cluster->ShiftBlockY(res_y - init_y);
      }
    } else {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow *lhs, const GriddedRow *rhs) {
                  return (lhs->LLY() < rhs->LLY());
                });
      int cluster_contour = stripe.LLY();
      int res_y;
      int init_y;
      for (auto &cluster : cluster_list) {
        init_y = cluster->LLY();
        res_y = std::max(cluster_contour, cluster->LLY());
        cluster->SetLLY(res_y);
        cluster_contour = cluster->URY();
        cluster->ShiftBlockY(res_y - init_y);
      }
    }
  } else {
    std::sort(cluster_list.begin(), cluster_list.end(),
              [](const GriddedRow *lhs, const GriddedRow *rhs) {
                return (lhs->LLY() < rhs->LLY());
              });
    int cluster_contour = RegionBottom();
    int res_y;
    int init_y;
    for (auto &cluster : cluster_list) {
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

/****
 * Returns the wire-length cost of the small group from l-th element to r-th
 * element in this cluster "for each order, we keep the left and right
 * boundaries of the group and evenly distribute the cells inside the group.
 * Since we have the Single-Segment Clustering technique to take care of the
 * cell positions, we do not pay much attention to the exact positions of the
 * cells during Local Re-ordering." from "An Efficient and Effective Detailed
 * Placement Algorithm"
 * ****/
double StdClusterWellLegalizer::WireLengthCost(GriddedRow *cluster, int l,
                                               int r) {
  auto &net_list = ckt_ptr_->Nets();
  std::set<Net *> net_involved;
  for (int i = l; i <= r; ++i) {
    auto *blk = cluster->Blocks()[i];
    for (auto &net_num : blk->NetList()) {
      if (net_list[net_num].PinCnt() < 100) {
        net_involved.insert(&(net_list[net_num]));
      }
    }
  }

  double hpwl_x = 0;
  double hpwl_y = 0;
  for (auto &net : net_involved) {
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
    std::vector<Block *> &res, double &cost, GriddedRow *cluster, int cur,
    int l, int r, int left_bound, int right_bound, int gap, int range) {
  // BOOST_LOG_TRIVIAL(info)  <<"l : %d, r: %d\n", l, r);
  if (cur == r) {
    cluster->Blocks()[l]->SetLLX(left_bound);
    cluster->Blocks()[r]->SetURX(right_bound);

    int left_contour = left_bound + gap + cluster->Blocks()[l]->Width();
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->Blocks()[i];
      blk->SetLLX(left_contour);
      left_contour += blk->Width() + gap;
    }

    double tmp_cost = WireLengthCost(cluster, l, r);
    if (tmp_cost < cost) {
      cost = tmp_cost;
      for (int j = 0; j < range; ++j) {
        res[j] = cluster->Blocks()[l + j];
      }
    }
  } else {
    // Permutations made
    auto &blk_list = cluster->Blocks();
    for (int i = cur; i <= r; ++i) {
      // Swapping done
      std::swap(blk_list[cur], blk_list[i]);

      // Recursion called
      FindBestLocalOrder(res, cost, cluster, cur + 1, l, r, left_bound,
                         right_bound, gap, range);

      // backtrack
      std::swap(blk_list[cur], blk_list[i]);
    }
  }
}

void StdClusterWellLegalizer::LocalReorderInCluster(GriddedRow *cluster,
                                                    int range) {
  /****
   * Enumerate all local permutations, @param range determines how big the local
   * range is
   * ****/

  assert(range > 0);

  int sz = cluster->Blocks().size();
  if (sz < 3) return;

  std::sort(cluster->Blocks().begin(), cluster->Blocks().end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });

  int last_segment = sz - range;
  std::vector<Block *> res_local_order(range, nullptr);
  for (int l = 0; l <= last_segment; ++l) {
    int tot_blk_width = 0;
    for (int j = 0; j < range; ++j) {
      res_local_order[j] = cluster->Blocks()[l + j];
      tot_blk_width += res_local_order[j]->Width();
    }
    int r = l + range - 1;
    double best_cost = DBL_MAX;
    int left_bound = (int)cluster->Blocks()[l]->LLX();
    int right_bound = (int)cluster->Blocks()[r]->URX();
    int gap = (right_bound - left_bound - tot_blk_width) / (r - l);

    FindBestLocalOrder(res_local_order, best_cost, cluster, l, l, r, left_bound,
                       right_bound, gap, range);
    for (int j = 0; j < range; ++j) {
      cluster->Blocks()[l + j] = res_local_order[j];
    }

    cluster->Blocks()[l]->SetLLX(left_bound);
    cluster->Blocks()[r]->SetURX(right_bound);
    int left_contour = left_bound + cluster->Blocks()[l]->Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      auto *blk = cluster->Blocks()[i];
      blk->SetLLX(left_contour);
      left_contour += blk->Width() + gap;
    }
  }
}

void StdClusterWellLegalizer::LocalReorderAllClusters() {
  // sort all cluster based on their lower left corners
  size_t tot_cluster_count = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  std::vector<GriddedRow *> cluster_ptr_list(tot_cluster_count, nullptr);
  int counter = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      for (auto &cluster : stripe.gridded_rows_) {
        cluster_ptr_list[counter] = &cluster;
        ++counter;
      }
    }
  }
  std::sort(cluster_ptr_list.begin(), cluster_ptr_list.end(),
            [](const GriddedRow *lhs, const GriddedRow *rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  for (auto &cluster_ptr : cluster_ptr_list) {
    LocalReorderInCluster(cluster_ptr, 3);
  }
}

/*
void StdClusterWellLegalizer::SingleSegmentClusteringOptimization() {
  BOOST_LOG_TRIVIAL(info) << "Start single segment clustering\n";

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.cluster_list_) {
        int num_old_cluster = cluster.blk_list_.size();
        std::vector<BlockSegment> old_cluster(num_old_cluster);
        for (int i = 0; i < num_old_cluster; ++i) {
          old_cluster[i].blk_index.push_back(i);
          old_cluster[i].circuit_ptr_ = circuit_ptr_;
          old_cluster[i].cluster_ = &cluster;
          old_cluster[i].UpdateBoundList();
          old_cluster[i].SortBounds();
          old_cluster[i].UpdateLLX();
        }

        bool is_overlap = false;
        do {
          std::vector<BlockSegment> new_cluster;
          new_cluster.push_back(old_cluster[0]);
          int j = 0;
          while (j + 1 < num_old_cluster) {
            if (new_cluster.back().Overlap(old_cluster[j + 1])) {
              new_cluster.back().Merge(old_cluster[j + 1]);
            } else {
              new_cluster.push_back(old_cluster[j + 1]);
            }
            j += 1;
          }
          int new_count = new_cluster.size();
          num_old_cluster = new_count;
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

          //BOOST_LOG_TRIVIAL(info)   << is_overlap << "\n";

        } while (is_overlap);

        for (int i = 0; i < num_old_cluster; ++i) {
          old_cluster[i].UpdateBlockLocation();
        }
      }
    }
  }

}
 */

void StdClusterWellLegalizer::UpdateClusterOrient() {
  for (auto &col : col_list_) {
    bool is_orient_N = is_first_row_orient_N_;
    for (auto &stripe : col.stripe_list_) {
      if (stripe.is_bottom_up_) {
        for (auto &cluster : stripe.gridded_rows_) {
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
  ckt_ptr_->design().WellTapCellCollection().Clear();
  size_t tot_cluster_count = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  ckt_ptr_->design().WellTapCellCollection().Reserve(tot_cluster_count * 2);

  int counter = 0;
  int tot_tap_cell_num = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      for (auto &row : stripe.gridded_rows_) {
        int tap_cell_num = 2;
        tot_tap_cell_num += tap_cell_num;
        int step = row.Width();
        int tap_cell_loc = row.LLX() - well_tap_cell_ptr_->Width() / 2;
        for (int i = 0; i < tap_cell_num; ++i) {
          std::string block_name = "__well_tap__" + std::to_string(counter++);
          Block &tap_cell =
              ckt_ptr_->design().WellTapCellCollection().CreateInstance(
                  block_name);
          tap_cell.SetPlacementStatus(PLACED);
          tap_cell.SetType(well_tap_cell_ptr_);
          tap_cell.SetId(
              ckt_ptr_->design().WellTapCellCollection().GetInstanceIdByName(
                  block_name));
          row.InsertWellTapCell(tap_cell, tap_cell_loc);
          tap_cell_loc += step;
        }
        row.LegalizeLooseX(space_to_well_tap_);
      }
    }
  }

  ckt_ptr_->design().WellTapCellCollection().Freeze();
  BOOST_LOG_TRIVIAL(info) << "Insertion complete: " << tot_tap_cell_num
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
void StdClusterWellLegalizer::CreateEndCapCellTypes() {
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      for (auto &row : stripe.gridded_rows_) {
        std::tuple<int, int> np_height = {row.NHeight(), row.PHeight()};

        // Check if the end-cap cell type for this height combination already
        // exists
        if (pre_end_cap_cell_np_heights_to_type_id.find(np_height) ==
            pre_end_cap_cell_np_heights_to_type_id.end()) {
          // Create and register the pre end-cap cell type
          std::string pre_end_cap_cell_name =
              "pre_end_cap_n_height_" + std::to_string(row.NHeight()) +
              "_p_height_" + std::to_string(row.PHeight());
          int pre_end_cap_cell_type_id = ckt_ptr_->CreateEndCapCellType(
              pre_end_cap_cell_name, ckt_ptr_->tech().PreEndCapMinWidth(),
              row.NHeight(), row.PHeight());
          pre_end_cap_cell_np_heights_to_type_id[np_height] =
              pre_end_cap_cell_type_id;

          // Create and register the post end-cap cell type
          std::string post_end_cap_cell_name =
              "post_end_cap_n_height_" + std::to_string(row.NHeight()) +
              "_p_height_" + std::to_string(row.PHeight());
          int post_end_cap_cell_type_id = ckt_ptr_->CreateEndCapCellType(
              post_end_cap_cell_name, ckt_ptr_->tech().PostEndCapMinWidth(),
              row.NHeight(), row.PHeight());
          post_end_cap_cell_np_heights_to_type_id[np_height] =
              post_end_cap_cell_type_id;
        }
      }
    }
  }
  ckt_ptr_->tech().EndCapCellTypeCollection().Freeze();
}

void StdClusterWellLegalizer::InsertEndCapCells() {
  // Clear all existing instances
  ckt_ptr_->design().EndCapCellCollection().Clear();
  size_t tot_cluster_count = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      tot_cluster_count += stripe.gridded_rows_.size();
    }
  }
  ckt_ptr_->design().EndCapCellCollection().Reserve(tot_cluster_count * 2);

  int row_counter = 0;
  int total_num_end_cap_cells = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      for (auto &row : stripe.gridded_rows_) {
        std::tuple<int, int> np_height = {row.NHeight(), row.PHeight()};

        // Create pre end cap cell
        int pre_end_cap_cell_type_id =
            pre_end_cap_cell_np_heights_to_type_id[np_height];
        BlockType *pre_end_cap_cell_type_ptr =
            ckt_ptr_->tech().EndCapCellTypeCollection().GetInstanceById(
                pre_end_cap_cell_type_id);
        int pre_end_cap_cell_loc =
            row.LLX() - pre_end_cap_cell_type_ptr->Width() / 2;
        std::string pre_end_cap_cell_name =
            "__pre_end_cap_cell__" + std::to_string(row_counter);
        Block &pre_end_cap_cell =
            ckt_ptr_->design().EndCapCellCollection().CreateInstance(
                pre_end_cap_cell_name);
        pre_end_cap_cell.SetPlacementStatus(PLACED);
        pre_end_cap_cell.SetType(pre_end_cap_cell_type_ptr);
        pre_end_cap_cell.SetId(
            ckt_ptr_->design().EndCapCellCollection().GetInstanceIdByName(
                pre_end_cap_cell_name));
        row.InsertWellTapCell(pre_end_cap_cell, pre_end_cap_cell_loc);

        // Create post end cap cell
        int post_end_cap_cell_type_id =
            pre_end_cap_cell_np_heights_to_type_id[np_height];
        BlockType *post_end_cap_cell_type_ptr =
            ckt_ptr_->tech().EndCapCellTypeCollection().GetInstanceById(
                post_end_cap_cell_type_id);
        int post_end_cap_cell_loc =
            row.URX() + post_end_cap_cell_type_ptr->Width() / 2;
        std::string post_end_cap_cell_name =
            "__post_end_cap_cell__" + std::to_string(row_counter);
        Block &post_end_cap_cell =
            ckt_ptr_->design().EndCapCellCollection().CreateInstance(
                post_end_cap_cell_name);
        post_end_cap_cell.SetPlacementStatus(PLACED);
        post_end_cap_cell.SetType(post_end_cap_cell_type_ptr);
        post_end_cap_cell.SetId(
            ckt_ptr_->design().EndCapCellCollection().GetInstanceIdByName(
                post_end_cap_cell_name));
        row.InsertWellTapCell(post_end_cap_cell, post_end_cap_cell_loc);

        total_num_end_cap_cells += 2;
        row_counter += 1;

        row.LegalizeLooseX(space_to_well_tap_);
      }
    }
  }

  ckt_ptr_->design().EndCapCellCollection().Freeze();
  BOOST_LOG_TRIVIAL(info) << "Insertion complete: " << total_num_end_cap_cells
                          << " pre- and post- end cap cells created\n";
}

void StdClusterWellLegalizer::ClearCachedData() {
  for (auto &block : ckt_ptr_->Blocks()) {
    block.SetOrient(N);
  }

  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
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
  is_success = BlockClusteringLoose();
  // BlockClusteringCompact();
  ReportHPWL();

  if (is_success) {
    BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                            << "Standard Cluster Well Legalization complete!\n"
                            << "\033[0m";
  } else {
    BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                            << "Standard Cluster Well Legalization fail!\n"
                            << "\033[0m";
  }

  return is_success;
}

bool StdClusterWellLegalizer::StartPlacement() {
  PrintStartStatement("standard cluster well legalization");

  // circuit_ptr_->GenMATLABWellTable("lg", false);

  bool is_success = true;
  InitializeWellLegalizer();
  BOOST_LOG_TRIVIAL(info) << "Form block clustering\n";
  // BlockClustering();
  // is_success = BlockClusteringCompact();
  is_success = BlockClusteringLoose();
  ReportHPWL();
  // circuit_ptr_->GenMATLABWellTable("clu", false);
  // GenMatlabClusterTable("clu_result");

  if (disable_cell_flip_) {
    BOOST_LOG_TRIVIAL(info) << "Skip flipping cluster orientation\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "Flip cluster orientation\n";
    UpdateClusterOrient();
    ReportHPWL();
    // circuit_ptr_->GenMATLABWellTable("ori", false);
    // GenMatlabClusterTable("ori_result");
  }

  BOOST_LOG_TRIVIAL(info) << "Perform local reordering\n";
  for (int i = 0; i < 6; ++i) {
    BOOST_LOG_TRIVIAL(info) << "reorder iteration: " << i << "\n";
    LocalReorderAllClusters();
    ReportHPWL();
    // BOOST_LOG_TRIVIAL(info) << "optimization: " << i;
    // SingleSegmentClusteringOptimization();
    // ReportHPWL();
  }
  // circuit_ptr_->GenMATLABWellTable("lop", false);
  // GenMatlabClusterTable("lop_result");

  if (disable_welltap_) {
    BOOST_LOG_TRIVIAL(info) << "Skip inserting well tap cells\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "Insert well tap cells\n";
    InsertWellTap();
    // circuit_ptr_->GenMATLABWellTable("wtc", false);
    // GenMatlabClusterTable("wtc_result");
  }

  if (enable_end_cap_cell_) {
    BOOST_LOG_TRIVIAL(info) << "Create end cap cells\n";
    CreateEndCapCellTypes();
    InsertEndCapCells();
  } else {
    BOOST_LOG_TRIVIAL(info) << "Skip creating end cap cells\n";
  }

  PrintEndStatement("Standard Cluster Well Legalization", is_success);
  // ReportEffectiveSpaceUtilization();

  return is_success;
}

void StdClusterWellLegalizer::ReportEffectiveSpaceUtilization() {
  int tot_std_blk_area = 0;
  int max_n_height = 0;
  int max_p_height = 0;
  for (auto &blk : ckt_ptr_->design().Blocks()) {
    BlockType *type = blk.TypePtr();
    if (type == ckt_ptr_->tech().IoDummyBlkTypePtr()) continue;
    ;
    if (type->Nheight() > max_n_height) {
      max_n_height = type->Nheight();
    }
    if (type->Pheight() > max_p_height) {
      max_p_height = type->Pheight();
    }
  }
  if (well_tap_cell_ptr_->Nheight() > max_n_height) {
    max_n_height = well_tap_cell_ptr_->Nheight();
  }
  if (well_tap_cell_ptr_->Pheight() > max_p_height) {
    max_p_height = well_tap_cell_ptr_->Pheight();
  }
  int max_height = max_n_height + max_p_height;

  int tot_eff_blk_area = 0;
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      for (auto &cluster : stripe.gridded_rows_) {
        int eff_height = cluster.Height();
        int tot_cell_width = 0;
        for (auto &blk_ptr : cluster.Blocks()) {
          tot_cell_width += blk_ptr->Width();
        }
        tot_eff_blk_area += tot_cell_width * eff_height;
        tot_std_blk_area += tot_cell_width * max_height;
      }
    }
  }
  double factor = ckt_ptr_->GridValueX() * ckt_ptr_->GridValueY();
  BOOST_LOG_TRIVIAL(info) << "Total placement area: "
                          << (RegionWidth() * RegionHeight()) * factor
                          << " um^2\n";
  BOOST_LOG_TRIVIAL(info) << "Total block area: "
                          << ckt_ptr_->TotBlkArea() * factor << " ("
                          << ckt_ptr_->TotBlkArea() / (double)RegionWidth() /
                                 (double)RegionHeight()
                          << ") um^2\n";
  BOOST_LOG_TRIVIAL(info) << "Total effective block area: "
                          << tot_eff_blk_area * factor << " ("
                          << tot_eff_blk_area / (double)RegionWidth() /
                                 (double)RegionHeight()
                          << ") um^2\n";
  BOOST_LOG_TRIVIAL(info) << "Total standard block area (lower bound):"
                          << tot_std_blk_area * factor << " ("
                          << tot_std_blk_area / (double)RegionWidth() /
                                 (double)RegionHeight()
                          << ") um^2\n";
}

void StdClusterWellLegalizer::GenMatlabClusterTable(
    std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  ckt_ptr_->GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

void StdClusterWellLegalizer::GenMATLABWellTable(
    std::string const &name_of_file, int well_emit_mode) {
  ckt_ptr_->GenMATLABWellTable(name_of_file, false);

  GenMATLABWellFillingTable(name_of_file, col_list_, RegionBottom(),
                            RegionTop(), well_emit_mode);

  GenPPNP(name_of_file);
}

void StdClusterWellLegalizer::GenPPNP(const std::string &name_of_file) {
  std::string np_file = name_of_file + "_np.txt";
  std::ofstream ostnp(np_file.c_str());
  DaliExpects(ostnp.is_open(), "Cannot open output file: " + np_file);

  std::string pp_file = name_of_file + "_pp.txt";
  std::ofstream ostpp(pp_file.c_str());
  DaliExpects(ostpp.is_open(), "Cannot open output file: " + pp_file);

  int adjust_width = well_tap_cell_ptr_->Width();

  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster : stripe.gridded_rows_) {
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
      for (auto &cluster : stripe.gridded_rows_) {
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
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
void StdClusterWellLegalizer::EmitDEFWellFile(std::string const &name_of_file,
                                              int well_emit_mode,
                                              bool enable_emitting_cluster) {
  EmitPPNPRect(name_of_file + "ppnp.rect");
  EmitWellRect(name_of_file + "well.rect", well_emit_mode);
  if (enable_emitting_cluster) {
    EmitClusterRect(name_of_file + "_router.cluster");
  }
}

void StdClusterWellLegalizer::EmitPPNPRect(std::string const &name_of_file) {
  // emit rect file
  std::string NP_name = "nplus";
  std::string PP_name = "pplus";

  BOOST_LOG_TRIVIAL(info) << "Writing PP and NP rect file: " << name_of_file
                          << "\n";

  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = ckt_ptr_->DistanceScaleFactorX();
  double factor_y = ckt_ptr_->DistanceScaleFactorY();

  ost << "bbox " << ckt_ptr_->LocDali2PhydbX(RegionLeft()) << " "
      << ckt_ptr_->LocDali2PhydbY(RegionBottom()) << " "
      << ckt_ptr_->LocDali2PhydbX(RegionRight()) << " "
      << ckt_ptr_->LocDali2PhydbY(RegionTop()) << "\n";

  int adjust_width = well_tap_cell_ptr_->Width();

  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster : stripe.gridded_rows_) {
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
      for (auto &cluster : stripe.gridded_rows_) {
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
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

void StdClusterWellLegalizer::ExportPpNpToPhyDB(phydb::PhyDB *phydb_ptr) {
  if (disable_welltap_) {
    BOOST_LOG_TRIVIAL(info) << "Skip export Pplus/Nplus fillings to PhyDB "
                               "since well tap is disabled\n";
    return;
  }
  DaliExpects(phydb_ptr != nullptr, "Cannot export plus layer to a nullptr");
  BOOST_LOG_TRIVIAL(info) << "Export Pplus/Nplus fillings to PhyDB\n";
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

  auto *phydb_layout_container = phydb_ptr->CreatePpNpMacroAndComponent(
      bbox_llx, bbox_lly, bbox_urx, bbox_ury);

  int adjust_width = well_tap_cell_ptr_->Width();

  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster : stripe.gridded_rows_) {
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
      for (auto &cluster : stripe.gridded_rows_) {
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
        } else {
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->URY());
          well_tap_top_bottom_list.push_back(cluster.Blocks()[0]->LLY());
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

void StdClusterWellLegalizer::EmitWellRect(std::string const &name_of_file,
                                           int well_emit_mode) {
  // emit rect file
  switch (well_emit_mode) {
    case 0: {
      BOOST_LOG_TRIVIAL(info)
          << "Writing N/P-well rect file: " << name_of_file << "\n";
      break;
    }
    case 1: {
      BOOST_LOG_TRIVIAL(info)
          << "Writing N-well rect file: " << name_of_file << "\n";
      break;
    }
    case 2: {
      BOOST_LOG_TRIVIAL(info)
          << "Writing P-well rect file: " << name_of_file << "\n";
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
  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      std::vector<int> pn_edge_list;
      if (stripe.is_bottom_up_) {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
        pn_edge_list.push_back(RegionTop());
      }
      for (auto &cluster : stripe.gridded_rows_) {
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

void StdClusterWellLegalizer::ExportWellToPhyDB(phydb::PhyDB *phydb_ptr,
                                                int well_emit_mode) {
  if (disable_welltap_) {
    BOOST_LOG_TRIVIAL(info)
        << "Skip export wells to PhyDB since well tap is disabled\n";
    return;
  }
  switch (well_emit_mode) {
    case 0:
      BOOST_LOG_TRIVIAL(info) << "Export N/P wells to PhyDB\n";
      break;
    case 1:
      BOOST_LOG_TRIVIAL(info) << "Export N wells to PhyDB\n";
      break;
    case 2:
      BOOST_LOG_TRIVIAL(info) << "Export P wells tp PhyDB\n";
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

  auto *phydb_layout_container = phydb_ptr->CreateWellLayerMacroAndComponent(
      bbox_llx, bbox_lly, bbox_urx, bbox_ury);

  for (auto &col : col_list_) {
    for (auto &stripe : col.stripe_list_) {
      std::vector<RectI> n_rects;
      std::vector<RectI> p_rects;
      CollectWellFillingRects(stripe, RegionBottom(), RegionTop(), n_rects,
                              p_rects);
      if (well_emit_mode != 1) {
        std::string signal_name = "GND";
        std::string layer_name = "pwell";
        for (auto &rect : p_rects) {
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
        for (auto &rect : p_rects) {
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
void StdClusterWellLegalizer::EmitClusterRect(std::string const &name_of_file) {
  BOOST_LOG_TRIVIAL(info) << "Writing cluster rect file: " << name_of_file
                          << "\n";
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = ckt_ptr_->DistanceScaleFactorX();
  double factor_y = ckt_ptr_->DistanceScaleFactorY();
  for (size_t i = 0; i < col_list_.size(); ++i) {
    std::string column_name = "column" + std::to_string(i);
    ost << "STRIP " << column_name << "\n";

    auto &col = col_list_[i];
    for (auto &stripe : col.stripe_list_) {
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
        for (auto &cluster : stripe.gridded_rows_) {
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
          auto &cluster = stripe.gridded_rows_[j];
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
