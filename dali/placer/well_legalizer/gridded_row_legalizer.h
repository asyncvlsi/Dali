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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LEGALIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LEGALIZER_H_

#include <phydb/phydb.h>

#include "dali/placer/displacement_viewer.h"
#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/component_legalization_state.h"
#include "dali/placer/well_legalizer/space_partitioner.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Multi-row well legalizer for gridded-cell placement. */
class GriddedRowLegalizer : public Placer {
  friend class ExtendedTetrisLegalizer;

 public:
  GriddedRowLegalizer() = default;

  /** Verify that required well data exists on the circuit. */
  void CheckWellInfo();

  /** Set worker thread count. */
  void SetNumThreads(int number_of_threads);

  /** Inject an external space partitioner. */
  void SetExternalSpacePartitioner(SpacePartitioner* p_external_partitioner);

  /** Set built-in partitioning mode. */
  void SetPartitionMode(int partitioning_mode_);

  /** Set maximum legalized row width. */
  void SetMaxRowWidth(double max_row_width);

  /** Partition placement area and components into legalization stripes. */
  void PartitionSpaceAndComponents();

  void SetWellTapCellParameters(bool is_well_tap_needed = true,
                                bool is_checker_board_mode = false,
                                double tap_cell_interval_microns = -1,
                                std::string const& well_tap_macro_name = "");

  void PrecomputeWellTapCellLocation();

  void InitializeComponentAuxiliaryInfo();
  void SaveInitialLoc();
  void SaveUpDownLoc();
  void SaveConsensusLoc();
  void RestoreInitialLocX();
  void RestoreGreedyLocX();
  void RestoreConsensusLocX();

  void SetLegalizationMaxIteration(int max_iteration);
  /** Legalize a stripe growing rows upward; the downward and disp-check
   * variants differ in direction and whether a displacement limit applies. */
  bool StripeLegalizationUpward(Stripe& stripe, bool use_init_loc);
  /** Legalize a stripe growing rows downward. */
  bool StripeLegalizationDownward(Stripe& stripe, bool use_init_loc);
  void CleanUpTemporaryRowSegments();
  /** Legalize by trying upward then downward and keeping the better result.
   * @return true if the stripe fits. */
  bool UpwardDownwardLegalization(bool use_init_loc = true);

  /** Upward stripe legalization that rejects cells exceeding a displacement limit. */
  bool StripeLegalizationUpwardWithDispCheck(Stripe& stripe, bool use_init_loc);
  bool StripeLegalizationDownwardWithDispCheck(Stripe& stripe,
                                               bool use_init_loc);
  /** Upward/downward legalization under a per-cell displacement limit. */
  bool UpwardDownwardLegalizationWithDispCheck(bool use_init_loc);

  bool IsLeftmostPlacementLegal();
  bool IsPlacementLegal();
  bool IterativeDisplacementOptimization();

  void EmbodyWellTapCells();

  void ReportDisplacement();

  bool StartPlacement() override;

  /** Populate rows from PhyDB standard-cell row definitions. */
  void ImportStandardRowSegments(phydb::PhyDB& phydb);
  void AssignStandardCellsToRowSegments();
  void ReportStandardCellDisplacement();
  bool StartStandardLegalization();

  void ReportOutOfBoundCell();

  void ReportEffectiveDensity();

 private:
  // space partitioner
  int partitioning_mode_ = 0;
  int max_row_width_ = -1;
  SpacePartitioner* space_partitioner_ = nullptr;

  int well_spacing_ = 0;
  int well_tap_p_height_ = 0;
  int well_tap_n_height_ = 0;

  std::vector<StripeColumn> col_list_;

  bool is_well_tap_needed_ = true;
  bool is_checker_board_mode_ = false;
  int tap_cell_interval_grid_ = -1;
  Macro* well_tap_macro_ = nullptr;

  int greedy_cur_iter_ = 0;
  int greedy_max_iter_ = 30;

  int consensus_max_iter_ = 1000;

  bool is_init_loc_cached_ = false;
  bool is_greedy_loc_cached_ = false;
  bool is_cons_loc_cached_ = false;
  std::vector<ComponentLegalizationState> component_auxs_;

  int number_of_threads_ = 1;

  void SetWellTapCellNecessary(bool is_well_tap_needed);
  void SetWellTapCellPlacementMode(bool is_checker_board_mode);
  void SetWellTapCellInterval(double tap_cell_interval_microns);
  void SetWellTapMacro(std::string const& well_tap_macro_name);
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LEGALIZER_H_
