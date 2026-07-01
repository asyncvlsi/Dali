/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_DETAILED_PLACER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_DETAILED_PLACER_H_

#include <string>
#include <vector>

#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/gridded_row.h"

namespace dali {

/**
 * Detailed placement stage for legalized gridded-cell rows.
 *
 * Gridded-cell detailed placement has a different legality model from
 * standard-cell detailed placement because cross-row moves may change required
 * P/N-well heights. This placer owns the gridded-specific sequence and starts
 * with local re-ordering, which is always legal inside one GriddedRow.
 */
class GriddedDetailedPlacer : public Placer {
 public:
  /** Attach the current legalized gridded rows. */
  void SetRows(std::vector<GriddedRow*> rows);

  /** Run gridded detailed placement on the attached rows. */
  bool StartPlacement() override;

 private:
  static constexpr int kLocalReorderWindowSize = 3;
  static constexpr int kMaxLocalReorderIterations = 6;
  static constexpr int kMaxDetailedIterations = 2;
  static constexpr int kMaxSwapCandidatesPerRowPair = 1;
  static constexpr int kMaxGlobalRowOffset = 4;
  static constexpr double kMinSignificantHpwlImprovement = 1e-9;

  struct SwapStats {
    int candidates = 0;
    int accepted = 0;
  };

  double WireLengthCost(GriddedRow* row, int left_index, int right_index) const;
  void FindBestLocalOrder(std::vector<Component*>& result, double& cost,
                          GriddedRow* row, int current_index, int left_index,
                          int right_index, int left_bound, int right_bound,
                          int gap, int window_size) const;
  int LocalReorderInRow(GriddedRow* row, int window_size) const;
  int LocalReorderAllRows();
  int RunLocalReorderStage();

  bool IsSwapCandidate(Component* component) const;
  int UsedWidthAfterSwap(GriddedRow* row, Component* removed,
                         Component* added) const;
  int RequiredPHeightAfterSwap(GriddedRow* row, Component* removed,
                               Component* added) const;
  int RequiredNHeightAfterSwap(GriddedRow* row, Component* removed,
                               Component* added) const;
  bool IsNonHeightIncreasingSwap(GriddedRow* first_row,
                                 Component* first_component,
                                 GriddedRow* second_row,
                                 Component* second_component) const;
  double RowPairWireLengthCost(GriddedRow* first_row,
                               GriddedRow* second_row) const;
  void PlaceComponentInRow(GriddedRow* row, Component* component) const;
  void LegalizeRowsAfterSwap(GriddedRow* first_row, GriddedRow* second_row);
  bool TrySwap(GriddedRow* first_row, int first_index, GriddedRow* second_row,
               int second_index);
  SwapStats TryClosestComponentSwaps(GriddedRow* first_row,
                                     GriddedRow* second_row,
                                     int max_candidates);
  SwapStats RunVerticalSwapStage();
  SwapStats RunGlobalSwapStage();
  void LogSwapStage(const std::string& stage_name, const SwapStats& stats,
                    double hpwl_before);

  std::vector<GriddedRow*> rows_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_DETAILED_PLACER_H_
