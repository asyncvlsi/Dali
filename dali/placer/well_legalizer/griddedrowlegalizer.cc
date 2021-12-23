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
#include "griddedrowlegalizer.h"

#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/memory.h"
#include "dali/common/timing.h"
#include "helper.h"

namespace dali {

void GriddedRowLegalizer::CheckWellInfo() {
  for (auto &multi_well: p_ckt_->tech().MultiWells()) {
    multi_well.CheckLegality();
  }

  Tech &tech = p_ckt_->tech();
  WellLayer &n_well_layer = tech.NwellLayer();
  double grid_value_x = p_ckt_->GridValueX();
  int same_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_spacing = std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_spacing, op_spacing);

  auto *tap_cell_well = well_tap_type_ptr_->WellPtr();
  tap_cell_p_height_ = tap_cell_well->PwellHeight(0);
  tap_cell_n_height_ = tap_cell_well->NwellHeight(0);
}

void GriddedRowLegalizer::SetExternalSpacePartitioner(
    AbstractSpacePartitioner *p_external_partitioner
) {
  space_partitioner_ = p_external_partitioner;
}

void GriddedRowLegalizer::SetPartitionMode(int partitioning_mode) {
  partitioning_mode_ = partitioning_mode;
}

void GriddedRowLegalizer::SetWellTapCellParameters(
    double tap_cell_interval_microns,
    bool is_checker_board_mode,
    std::string const &well_tap_type_name
) {
  BOOST_LOG_TRIVIAL(info)
    << "Provided well tap cell interval:      "
    << tap_cell_interval_microns << " um\n";
  BOOST_LOG_TRIVIAL(info)
    << "Gridded value x:                      "
    << p_ckt_->GridValueX() << " um\n";
  tap_cell_interval_grid_ =
      std::floor(tap_cell_interval_microns / p_ckt_->GridValueX());
  BOOST_LOG_TRIVIAL(info)
    << "Well tap cell interval in grid unit : "
    << tap_cell_interval_grid_ << "\n";

  is_checker_board_mode_ = is_checker_board_mode;
  std::string is_mode_on = is_checker_board_mode_ ? "True" : "False";
  BOOST_LOG_TRIVIAL(info) << "Checkerboard mode on: " << is_mode_on << "\n";

  if (well_tap_type_name.empty()) {
    BOOST_LOG_TRIVIAL(info) << "Well tap cell type not specified\n";
    DaliExpects(!p_ckt_->tech().WellTapCellPtrs().empty(),
                "No well tap cells provided in the cell library?");
    well_tap_type_ptr_ = p_ckt_->tech().WellTapCellPtrs()[0];
    BOOST_LOG_TRIVIAL(info) << "Using the default well tap cell: "
                            << well_tap_type_ptr_->Name() << "\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "Provided well tap cell type: "
                            << well_tap_type_name << "\n";
    well_tap_type_ptr_ = p_ckt_->GetBlockTypePtr(well_tap_type_name);
  }
}

void GriddedRowLegalizer::PartitionSpaceAndBlocks() {
  bool is_external_partitioner_provided = (space_partitioner_ != nullptr);
  if (!is_external_partitioner_provided) {
    space_partitioner_ = new DefaultSpacePartitioner;
  }

  space_partitioner_->SetInputCircuit(p_ckt_);
  space_partitioner_->SetOutput(&col_list_);
  space_partitioner_->SetReservedSpaceToBoundaries(
      well_spacing_, well_spacing_, 1, 1
  );
  space_partitioner_->SetPartitionMode(partitioning_mode_);
  space_partitioner_->StartPartitioning();

  if (!is_external_partitioner_provided) {
    delete space_partitioner_;
    space_partitioner_ = nullptr;
  }
}

void GriddedRowLegalizer::SetLegalizationMaxIteration(int max_iteration) {
  max_iteration_ = max_iteration;
}

bool GriddedRowLegalizer::StripeLegalizationUpward(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = true;

  stripe.SortBlocksBasedOnYLocation(0);

  size_t processed_blk_cnt = 0;
  while (processed_blk_cnt < stripe.block_list_.size()) {
    stripe.UpdateFrontClusterUpward(tap_cell_p_height_, tap_cell_n_height_);
    processed_blk_cnt = stripe.FitBlocksToFrontSpaceUpward(
        processed_blk_cnt, cur_iter_
    );
    stripe.LegalizeFrontCluster();
  }
  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_, true);
  stripe.UpdateBlockYLocation();
  StretchBlocks();

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedRowLegalizer::StripeLegalizationDownward(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = false;

  stripe.SortBlocksBasedOnYLocation(2);

  size_t processed_blk_cnt = 0;
  while (processed_blk_cnt < stripe.block_list_.size()) {
    stripe.UpdateFrontClusterDownward(tap_cell_p_height_, tap_cell_n_height_);
    processed_blk_cnt = stripe.FitBlocksToFrontSpaceDownward(
        processed_blk_cnt, cur_iter_
    );
    stripe.LegalizeFrontCluster();
  }
  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_, false);
  stripe.UpdateBlockYLocation();
  StretchBlocks();

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedRowLegalizer::GroupBlocksToClusters() {
  bool res = true;
  for (ClusterStripe &col: col_list_) {
    bool is_success = true;
    for (Stripe &stripe: col.stripe_list_) {
      bool is_from_bottom = true;
      for (cur_iter_ = 0; cur_iter_ < max_iteration_; ++cur_iter_) {
        if (is_from_bottom) {
          is_success = StripeLegalizationUpward(stripe);
        } else {
          is_success = StripeLegalizationDownward(stripe);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
    }
  }
  return res;
}

void GriddedRowLegalizer::StretchBlocks() {
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      stripe.UpdateBlockStretchLength();
    }
  }
}

void GriddedRowLegalizer::EmbodyWellTapCells() {

}

bool GriddedRowLegalizer::StartPlacement() {
  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start Gridded Row Well Legalization\n";

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  bool is_successful = true;

  CheckWellInfo();
  PartitionSpaceAndBlocks();
  bool is_success = GroupBlocksToClusters();
  EmbodyWellTapCells();

  ReportHPWL();

  if (is_success) {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m"
      << "Gridded Row Well Legalization complete!\n"
      << "\033[0m";
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m"
      << "Gridded Row Well Legalization fail!\n"
      << "Please a lower placement density\n"
      << "\033[0m";
  }

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";

  ReportMemory();

  return is_successful;
}

void GriddedRowLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

void GriddedRowLegalizer::GenMATLABWellTable(
    std::string const &name_of_file,
    int well_emit_mode
) {
  std::string frame_file = name_of_file + "_outline.txt";
  p_ckt_->GenMATLABWellTable(name_of_file, false);
  GenMATLABWellFillingTable(
      name_of_file, col_list_,
      RegionBottom(), RegionTop(),
      well_emit_mode
  );
  GenClusterTable(name_of_file, col_list_);
}

}
