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

#include <cmath>

#include "dali/common/config.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/memory.h"
#include "dali/common/timing.h"
#include "dali/placer/well_legalizer/optimizationhelper.h"
#include "dali/placer/well_legalizer/stripehelper.h"

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

void GriddedRowLegalizer::SetMaxRowWidth(double max_row_width) {
  BOOST_LOG_TRIVIAL(info)
    << "Provided max row width: " << max_row_width << " um\n";
  BOOST_LOG_TRIVIAL(info)
    << "Gridded value x: " << p_ckt_->GridValueX() << " um\n";
  if (max_row_width < 0) {
    max_row_width_ = RegionRight() - RegionLeft();
  } else {
    max_row_width_ = std::floor(max_row_width / p_ckt_->GridValueX());
  }
  BOOST_LOG_TRIVIAL(info)
    << "Max row width in grid unit : "
    << tap_cell_interval_grid_ << "\n";
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
  space_partitioner_->SetMaxRowWidth(max_row_width_);
  space_partitioner_->StartPartitioning();

  if (!is_external_partitioner_provided) {
    delete space_partitioner_;
    space_partitioner_ = nullptr;
  }
}

void GriddedRowLegalizer::SetWellTapCellParameters(
    bool is_well_tap_needed,
    bool is_checker_board_mode,
    double tap_cell_interval_microns,
    std::string const &well_tap_type_name
) {
  SetWellTapCellNecessary(is_well_tap_needed);
  SetWellTapCellPlacementMode(is_checker_board_mode);
  SetWellTapCellInterval(tap_cell_interval_microns);
  SetWellTapCellType(well_tap_type_name);
}

void GriddedRowLegalizer::PrecomputeWellTapCellLocation() {
  if (!is_well_tap_needed_) return;
  for (ClusterStripe &cluster: col_list_) {
    for (Stripe &stripe: cluster.stripe_list_) {
      stripe.PrecomputeWellTapCellLocation(
          is_checker_board_mode_, tap_cell_interval_grid_, well_tap_type_ptr_
      );
    }
  }
}

void GriddedRowLegalizer::InitializeBlockAuxiliaryInfo() {
  blk_auxs_.reserve(Blocks().size());
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    blk_auxs_.emplace_back(&blk);
  }
}

void GriddedRowLegalizer::SaveInitialLoc() {
  is_init_loc_cached_ = true;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->StoreCurLocAsInitLoc();
  }
}

void GriddedRowLegalizer::SaveGreedyLoc() {
  is_greedy_loc_cached_ = true;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->StoreCurLocAsGreedyLoc();
  }
}

void GriddedRowLegalizer::SaveQPLoc() {
  is_qp_loc_cached_ = true;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->StoreCurLocAsQPLoc();
  }
}

void GriddedRowLegalizer::SaveConsensusLoc() {
  is_cons_loc_cached_ = true;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->StoreCurLocAsConsLoc();
  }
}

void GriddedRowLegalizer::RestoreInitialLocX() {
  DaliExpects(is_init_loc_cached_,
              "Initial locations are not saved, no way to restore");
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->RecoverInitLocX();
  }
}

void GriddedRowLegalizer::RestoreGreedyLocX() {
  DaliExpects(is_greedy_loc_cached_,
              "Greedy locations are not saved, no way to restore");
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->RecoverGreedyLocX();
  }
}

void GriddedRowLegalizer::RestoreQPLocX() {
  DaliExpects(is_qp_loc_cached_,
              "Quadratic programming locations are not saved, no way to restore");
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->RecoverQPLocX();
  }
}

void GriddedRowLegalizer::RestoreConsensusLocX() {
  DaliExpects(is_cons_loc_cached_,
              "Consensus locations are not saved, no way to restore");
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    aux_ptr->RecoverConsLocX();
  }
}

void GriddedRowLegalizer::SetLegalizationMaxIteration(int max_iteration) {
  greedy_max_iter_ = max_iteration;
}

bool GriddedRowLegalizer::StripeLegalizationUpward(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = true;

  stripe.SortBlocksBasedOnYLocation(0);

  size_t processed_blk_cnt = 0;
  while (processed_blk_cnt < stripe.blk_ptrs_vec_.size()) {
    stripe.UpdateFrontClusterUpward(tap_cell_p_height_, tap_cell_n_height_);
    processed_blk_cnt = stripe.FitBlocksToFrontSpaceUpward(
        processed_blk_cnt, greedy_cur_iter_
    );
    stripe.LegalizeFrontCluster();
  }
  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_, true);
  stripe.UpdateBlockYLocation();
  stripe.UpdateBlockStretchLength();

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedRowLegalizer::StripeLegalizationDownward(Stripe &stripe) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = false;

  stripe.SortBlocksBasedOnYLocation(2);

  size_t processed_blk_cnt = 0;
  while (processed_blk_cnt < stripe.blk_ptrs_vec_.size()) {
    stripe.UpdateFrontClusterDownward(tap_cell_p_height_, tap_cell_n_height_);
    processed_blk_cnt = stripe.FitBlocksToFrontSpaceDownward(
        processed_blk_cnt, greedy_cur_iter_
    );
    stripe.LegalizeFrontCluster();
  }
  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_, false);
  stripe.UpdateBlockYLocation();
  stripe.UpdateBlockStretchLength();

  return stripe.HasNoRowsSpillingOut();
}

void GriddedRowLegalizer::CleanUpTemporaryRowSegments() {
  for (ClusterStripe &col: col_list_) {
    for (Stripe &stripe: col.stripe_list_) {
      stripe.CleanUpTemporaryRowSegments();
    }
  }
}

bool GriddedRowLegalizer::GreedyLegalization() {
  bool res = true;
  for (ClusterStripe &col: col_list_) {
    bool is_success = true;
    for (Stripe &stripe: col.stripe_list_) {
      bool is_from_bottom = true;
      for (greedy_cur_iter_ = 0; greedy_cur_iter_ < greedy_max_iter_;
           ++greedy_cur_iter_) {
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
  CleanUpTemporaryRowSegments();
  ReportDisplacement();
  return res;
}

bool GriddedRowLegalizer::IsLeftmostPlacementLegal() {
  bool res = true;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      bool tmp_res = stripe.IsLeftmostPlacementLegal();
      res = res && tmp_res;
    }
  }
  return res;
}

bool GriddedRowLegalizer::IsPlacementLegal() {
  bool res = true;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      bool tmp_res = stripe.IsStripeLegal();
      res = res && tmp_res;
    }
  }
  return res;
}

bool GriddedRowLegalizer::OptimizeDisplacementUsingQuadraticProgramming() {
#if DALI_USE_CPLEX
  BOOST_LOG_TRIVIAL(info)
    << "Optimizing displacement X using quadratic programming\n";
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  bool is_successful = true;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      bool res = stripe.OptimizeDisplacementUsingQuadraticProgramming();
      is_successful = res && is_successful;
    }
  }

  if (is_successful) {
    BOOST_LOG_TRIVIAL(info) << "Quadratic programming complete\n";
    ReportDisplacement();
  } else {
    BOOST_LOG_TRIVIAL(info) << "Quadratic programming solution not found\n";
  }

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";

  return is_successful;
#else
  BOOST_LOG_TRIVIAL(info)
    << "Skip optimizing displacement using quadratic programming: CPLEX not found\n";
  return true;
#endif
}

bool GriddedRowLegalizer::IterativeDisplacementOptimization() {
  BOOST_LOG_TRIVIAL(info)
    << "Optimizing displacement X using the consensus algorithm\n";
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  consensus_max_iter_ = 2;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      stripe.IterativeCellReordering(consensus_max_iter_);
    }
  }
  ReportDisplacement();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";

  return IsPlacementLegal();
}

void GriddedRowLegalizer::EmbodyWellTapCells() {
  if (!is_well_tap_needed_) return;

  // compute the number of well-tap cells and reserve space
  size_t well_tap_cell_count_upper_limit = 0;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      size_t upper_limit_per_row = std::max(
          stripe.well_tap_cell_location_even_.size(),
          stripe.well_tap_cell_location_odd_.size()
      );
      well_tap_cell_count_upper_limit +=
          upper_limit_per_row * stripe.gridded_rows_.size();
    }
  }
  auto &tap_cell_list = p_ckt_->design().WellTaps();
  tap_cell_list.clear();
  tap_cell_list.reserve(well_tap_cell_count_upper_limit);

  // create well-tap cells
  size_t counter = 0;
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      counter = stripe.AddWellTapCells(p_ckt_, well_tap_type_ptr_, counter);
    }
  }
}

void GriddedRowLegalizer::ReportDisplacement() {
  if (!is_init_loc_cached_) {
    BOOST_LOG_TRIVIAL(info)
      << "Initial locations are not saved, cannot compute displacement\n";
  }
  double disp_x = 0, disp_y = 0;
  double quadratic_disp_x = 0, quadratic_disp_y = 0;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(blk.LLX() - init_loc.x);
    double tmp_disp_y = std::fabs(blk.LLY() - init_loc.y);
    disp_x += tmp_disp_x;
    disp_y += tmp_disp_y;
    quadratic_disp_x += tmp_disp_x * tmp_disp_x;
    quadratic_disp_y += tmp_disp_y * tmp_disp_y;
  }
  disp_x *= p_ckt_->GridValueX();
  disp_y *= p_ckt_->GridValueY();
  quadratic_disp_x *= p_ckt_->GridValueX() * p_ckt_->GridValueX();
  quadratic_disp_y *= p_ckt_->GridValueY() * p_ckt_->GridValueY();
  BOOST_LOG_TRIVIAL(info) << "  Current linear displacement\n";
  BOOST_LOG_TRIVIAL(info) << "    x: " << disp_x
                          << ", y: " << disp_y
                          << ", sum: " << disp_x + disp_y << " um\n";
  BOOST_LOG_TRIVIAL(info) << "  Current quadratic displacement\n";
  BOOST_LOG_TRIVIAL(info) << "    x: " << quadratic_disp_x
                          << ", y: " << quadratic_disp_y
                          << ", sum: " << quadratic_disp_x + quadratic_disp_y
                          << " um^2\n";
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
  PrecomputeWellTapCellLocation();

  InitializeBlockAuxiliaryInfo();
  SaveInitialLoc();

  bool is_success = GreedyLegalization();
  SaveGreedyLoc();
  ReportHPWL();

  if (is_success) {
#if false
    RestoreInitialLocX();
    IsLeftmostPlacementLegal();
    //IterativeCellReordering();
    //bool is_qp_solved = OptimizeDisplacementUsingQuadraticProgramming();
    OptimizeDisplacementUsingQuadraticProgramming();
    SaveQPLoc();
    ReportHPWL();
#endif

#if true
    RestoreInitialLocX();
    //bool is_cons_solved = IterativeDisplacementOptimization();
    IterativeDisplacementOptimization();
    GenSubCellTable("subcell");
    //ReportHPWL();
    //GreedyLegalization();
    SaveConsensusLoc();
    ReportHPWL();
#endif

    EmbodyWellTapCells();

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

void GriddedRowLegalizer::ImportStandardRowSegments(phydb::PhyDB &phydb) {
  // create one stripe column and one stripe
  col_list_.emplace_back();
  auto &col = col_list_.back();
  col.stripe_list_.emplace_back();
  auto &stripe = col.stripe_list_.back();
  stripe.ImportStandardRowSegments(phydb, *p_ckt_);
  stripe.blk_ptrs_vec_.reserve(Blocks().size());
  for (auto &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    stripe.blk_ptrs_vec_.emplace_back(&blk);
  }
}

void GriddedRowLegalizer::AssignStandardCellsToRowSegments() {
  auto &stripe = col_list_[0].stripe_list_[0];
  stripe.AssignStandardCellsToRowSegments(p_ckt_->WhiteSpaceUsage());
}

void GriddedRowLegalizer::ReportStandardCellDisplacement() {
  if (!is_init_loc_cached_) {
    BOOST_LOG_TRIVIAL(info)
      << "Initial locations are not saved, cannot compute displacement\n";
  }
  double sum_disp_x = 0, sum_disp_y = 0;
  double max_disp_x = 0, max_disp_y = 0;
  double max_manhattan_disp = 0;
  double sum_eucli_disp = 0;
  double max_euclidean_disp = 0;
  int cell_count = 0;
  for (Block &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    auto aux_ptr = static_cast<LgBlkAux *>(blk.AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(blk.LLX() - init_loc.x);
    double tmp_disp_y = std::fabs(blk.LLY() - init_loc.y);
    sum_disp_x += tmp_disp_x;
    sum_disp_y += tmp_disp_y;
    max_disp_x = std::max(max_disp_x, tmp_disp_x);
    max_disp_y = std::max(max_disp_y, tmp_disp_y);
    max_manhattan_disp = std::max(max_manhattan_disp, tmp_disp_x + tmp_disp_y);

    double tmp_euclidean_disp =
        std::sqrt(tmp_disp_x * tmp_disp_x + tmp_disp_y * tmp_disp_y);
    sum_eucli_disp += tmp_euclidean_disp;
    max_euclidean_disp = std::max(
        max_euclidean_disp, tmp_euclidean_disp
    );
    ++cell_count;
  }
  BOOST_LOG_TRIVIAL(info) << "Standard cell legalization summary\n";
  BOOST_LOG_TRIVIAL(info) << "  sum manhattan displacement\n";
  BOOST_LOG_TRIVIAL(info) << "    x: " << sum_disp_x
                          << ", y: " << sum_disp_y
                          << ", sum: " << sum_disp_x + sum_disp_y << " sites\n";
  BOOST_LOG_TRIVIAL(info) << "  average manhattan displacement\n";
  BOOST_LOG_TRIVIAL(info) << "    x: " << sum_disp_x / cell_count
                          << ", y: " << sum_disp_y / cell_count
                          << ", sum: " << (sum_disp_x + sum_disp_y) / cell_count
                          << " sites\n";
  BOOST_LOG_TRIVIAL(info) << "  max manhattan displacement\n";
  BOOST_LOG_TRIVIAL(info) << "    x: " << max_disp_x
                          << ", y: " << max_disp_y
                          << " sites\n";
  BOOST_LOG_TRIVIAL(info) << "  max sum manhattan displacement: "
                          << max_manhattan_disp << " sites\n";

  BOOST_LOG_TRIVIAL(info) << "  sum euclidean displacement: "
                          << sum_eucli_disp << " sites\n";
  BOOST_LOG_TRIVIAL(info) << "  average euclidean displacement: "
                          << sum_eucli_disp / cell_count << " sites\n";
  BOOST_LOG_TRIVIAL(info) << "  max euclidean displacement: "
                          << max_euclidean_disp << " sites\n";
}

bool GriddedRowLegalizer::StartStandardLegalization() {
  AssignStandardCellsToRowSegments();
  IterativeDisplacementOptimization();
  ReportStandardCellDisplacement();
  ReportHPWL();
  ReportBoundingBox();
  GenSubCellTable("subcell");
  return true;
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

void GriddedRowLegalizer::GenSubCellTable(std::string const &name_of_file) {
  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost_cluster(cluster_file.c_str());
  DaliExpects(ost_cluster.is_open(),
              "Cannot open output file: " + name_of_file);

  std::string sub_cell_file = name_of_file + "_result.txt";
  std::ofstream ost_sub_cell(sub_cell_file.c_str());
  DaliExpects(ost_sub_cell.is_open(),
              "Cannot open output file: " + name_of_file);

  std::string discrepancy_file = name_of_file + "_disc.txt";
  std::ofstream ost_discrepancy(discrepancy_file.c_str());
  DaliExpects(ost_discrepancy.is_open(),
              "Cannot open output file: " + name_of_file);

  std::string displacement_file = name_of_file + "_disp.txt";
  std::ofstream ost_displacement(displacement_file.c_str());
  DaliExpects(ost_discrepancy.is_open(),
              "Cannot open output file: " + name_of_file);

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &row: stripe.gridded_rows_) {
        row.GenSubCellTable(
            ost_cluster,
            ost_sub_cell,
            ost_discrepancy,
            ost_displacement
        );
      }
    }
  }

}

void GriddedRowLegalizer::SetWellTapCellNecessary(bool is_well_tap_needed) {
  is_well_tap_needed_ = is_well_tap_needed;
  std::string is_needed = is_well_tap_needed_ ? "True" : "False";
  BOOST_LOG_TRIVIAL(info) << "Place well tap cells: " << is_needed << "\n";
}

void GriddedRowLegalizer::SetWellTapCellPlacementMode(
    bool is_checker_board_mode
) {
  is_checker_board_mode_ = is_checker_board_mode;
  std::string is_mode_on = is_checker_board_mode_ ? "True" : "False";
  BOOST_LOG_TRIVIAL(info) << "Checkerboard mode on: " << is_mode_on << "\n";
}

void GriddedRowLegalizer::SetWellTapCellInterval(
    double tap_cell_interval_microns
) {
  if (tap_cell_interval_microns > 0) {
    BOOST_LOG_TRIVIAL(info)
      << "Provided well tap cell interval: "
      << tap_cell_interval_microns << " um\n";
  } else {
    Tech &tech = p_ckt_->tech();
    WellLayer &n_well_layer = tech.NwellLayer();
    double max_unplug_length = n_well_layer.MaxPlugDist();
    if (is_checker_board_mode_) {
      tap_cell_interval_microns = 4 * max_unplug_length;
      BOOST_LOG_TRIVIAL(info)
        << "Using default well tap cell interval 4*max_unplug_length: "
        << tap_cell_interval_microns << " um\n";
    } else {
      tap_cell_interval_microns = 2 * max_unplug_length;
      BOOST_LOG_TRIVIAL(info)
        << "Using default well tap cell interval 2*max_unplug_length: "
        << tap_cell_interval_microns << " um\n";
    }
  }
  BOOST_LOG_TRIVIAL(info)
    << "Gridded value x: "
    << p_ckt_->GridValueX() << " um\n";
  tap_cell_interval_grid_ =
      std::floor(tap_cell_interval_microns / p_ckt_->GridValueX());
  BOOST_LOG_TRIVIAL(info)
    << "Well tap cell interval in grid unit : "
    << tap_cell_interval_grid_ << "\n";
}

void GriddedRowLegalizer::SetWellTapCellType(
    std::string const &well_tap_type_name
) {
  if (well_tap_type_name.empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Well tap cell type not specified\n";
    DaliExpects(!p_ckt_->tech().WellTapCellPtrs().empty(),
                "No well tap cells provided in the cell library?");
    well_tap_type_ptr_ = p_ckt_->tech().WellTapCellPtrs()[0];
    BOOST_LOG_TRIVIAL(info)
      << "Using the default well tap cell: "
      << well_tap_type_ptr_->Name() << "\n";
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "Provided well tap cell type: "
      << well_tap_type_name << "\n";
    well_tap_type_ptr_ = p_ckt_->GetBlockTypePtr(well_tap_type_name);
  }
}

}
