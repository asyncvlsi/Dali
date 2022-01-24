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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_

#include <phydb/phydb.h>

#include "dali/common/displaceviewer.h"
#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/lgblkaux.h"
#include "dali/placer/well_legalizer/spacepartitioner.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

class GriddedRowLegalizer : public Placer {
 public:
  GriddedRowLegalizer() = default;

  void CheckWellInfo();

  void SetExternalSpacePartitioner(AbstractSpacePartitioner *p_external_partitioner);
  void SetPartitionMode(int partitioning_mode_);
  void SetMaxRowWidth(double max_row_width);
  void PartitionSpaceAndBlocks();

  void SetWellTapCellParameters(
      bool is_well_tap_needed = true,
      bool is_checker_board_mode = false,
      double tap_cell_interval_microns = -1,
      std::string const &well_tap_type_name = ""
  );

  void PrecomputeWellTapCellLocation();

  void InitializeBlockAuxiliaryInfo();
  void SaveInitialLoc();
  void SaveGreedyLoc();
  void SaveQPLoc();
  void SaveConsensusLoc();
  void RestoreInitialLocX();
  void RestoreGreedyLocX();
  void RestoreQPLocX();
  void RestoreConsensusLocX();

  void SetLegalizationMaxIteration(int max_iteration);
  bool StripeLegalizationUpward(Stripe &stripe);
  bool StripeLegalizationDownward(Stripe &stripe);
  void CleanUpTemporaryRowSegments();
  bool GreedyLegalization();

  bool IsLeftmostPlacementLegal();
  bool IsPlacementLegal();
  bool OptimizeDisplacementUsingQuadraticProgramming();

  bool IterativeDisplacementOptimization();

  void EmbodyWellTapCells();

  void ReportDisplacement();

  bool StartPlacement() override;

  void ImportStandardRowSegments(phydb::PhyDB &phydb);
  void AssignStandardCellsToRowSegments();
  bool StartStandardLegalization();

  void GenMatlabClusterTable(std::string const &name_of_file);
  void GenMATLABWellTable(
      std::string const &name_of_file,
      int well_emit_mode
  ) override;
  void GenSubCellTable(std::string const &name_of_file);
 private:
  // space partitioner
  int partitioning_mode_ = 0;
  int max_row_width_ = -1;
  AbstractSpacePartitioner *space_partitioner_ = nullptr;

  int well_spacing_ = 0;
  int tap_cell_p_height_ = 0;
  int tap_cell_n_height_ = 0;

  std::vector<ClusterStripe> col_list_;

  bool is_well_tap_needed_ = true;
  bool is_checker_board_mode_ = false;
  int tap_cell_interval_grid_ = -1;
  BlockType *well_tap_type_ptr_ = nullptr;

  int greedy_cur_iter_ = 0;
  int greedy_max_iter_ = 10;

  int consensus_max_iter_ = 100;

  bool is_init_loc_cached_ = false;
  bool is_greedy_loc_cached_ = false;
  bool is_qp_loc_cached_ = false;
  bool is_cons_loc_cached_ = false;
  std::vector<LgBlkAux> blk_auxs_;

  void SetWellTapCellNecessary(bool is_well_tap_needed);
  void SetWellTapCellPlacementMode(bool is_checker_board_mode);
  void SetWellTapCellInterval(double tap_cell_interval_microns);
  void SetWellTapCellType(std::string const &well_tap_type_name);
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_
