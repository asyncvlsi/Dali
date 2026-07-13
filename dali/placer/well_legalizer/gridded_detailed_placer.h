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

#include <functional>
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
  /** Callback used by the application to emit visualization snapshots. */
  using SnapshotCallback =
      std::function<void(const std::string& id, const std::string& label,
                         const std::string& subgroup, int iteration)>;

  /** Attach the current legalized gridded rows. */
  void SetRows(std::vector<GriddedRow*> rows);

  /** Set a callback invoked after each gridded detailed-placement stage. */
  void SetSnapshotCallback(SnapshotCallback snapshot_callback);

  /** Run gridded detailed placement on the attached rows. */
  bool StartPlacement() override;

  /** Reorder cells within each row without attempting cross-row swaps. */
  bool StartLocalReorder();

 private:
  static constexpr int kLocalReorderWindowSize = 3;
  static constexpr int kMaxLocalReorderIterations = 6;
  static constexpr int kMaxDetailedIterations = 2;
  static constexpr int kMaxSwapCandidatesPerRowPair = 1;
  static constexpr int kMaxOptimalRegionRowsPerComponent = 4;
  static constexpr int kMaxOptimalRegionRowsPerStripe = 2;
  static constexpr int kMaxOptimalRegionCandidatesPerRow = 2;
  static constexpr double kMinSignificantHpwlImprovement = 1e-9;

  struct SwapStats {
    int candidates = 0;
    int accepted = 0;
  };

  struct OptimalRegion {
    bool valid = false;
    double lx = 0;
    double ly = 0;
    double ux = 0;
    double uy = 0;
  };

  struct RowStripe {
    std::vector<GriddedRow*> rows;
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
  double DistanceToOptimalRegionX(Component* component,
                                  const OptimalRegion& region) const;
  double DistanceToOptimalRegionY(GriddedRow* row, Component* component,
                                  const OptimalRegion& region) const;
  /** Estimate the closest legal X distance from a row to an optimal region. */
  double DistanceFromRowToOptimalRegionX(
      GriddedRow* row, Component* component,
      const OptimalRegion& region) const;
  OptimalRegion ComputeOptimalRegion(Component* component) const;
  void PlaceComponentInRow(GriddedRow* row, Component* component) const;
  void LegalizeRowsAfterSwap(GriddedRow* first_row, GriddedRow* second_row);
  bool TrySwap(GriddedRow* first_row, int first_index, GriddedRow* second_row,
               int second_index);
  SwapStats TryClosestComponentSwaps(GriddedRow* first_row,
                                     GriddedRow* second_row,
                                     int max_candidates);
  SwapStats TryOptimalRegionSwaps(GriddedRow* source_row, int source_index);
  SwapStats RunVerticalSwapStage();
  SwapStats RunGlobalSwapStage();
  void LogSwapStage(const std::string& stage_name, const SwapStats& stats,
                    double hpwl_before);
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& subgroup, int iteration);
  /** Group rows by stripe bounds and sort each stripe from bottom to top. */
  void BuildRowStripeIndex();

  std::vector<GriddedRow*> rows_;
  std::vector<RowStripe> row_stripes_;
  SnapshotCallback snapshot_callback_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_DETAILED_PLACER_H_
