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

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
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

  /** Set the safety cap on complete gridded detailed-placement rounds. */
  void SetMaxRounds(int max_rounds);

  /** Set the minimum relative HPWL gain required to start another round. */
  void SetMinRelativeImprovement(double min_relative_improvement);

  /** Enable or disable the within-stripe vertical-swap stage. */
  void SetEnableVerticalSwap(bool enable);

  /** Enable or disable exact cross-row moves into existing row whitespace. */
  void SetEnableRelocation(bool enable);

  /** Set the maximum optimal-region rows considered for one component. */
  void SetMaxCandidateRows(int max_candidate_rows);

  /** Set the pin-count cutoff for nets omitted from move proposals and costs.
   */
  void SetNetIgnoreThreshold(int net_ignore_threshold);

  /** Run gridded detailed placement on the attached rows. */
  bool StartPlacement() override;

  /** Reorder cells within each row without attempting cross-row swaps. */
  bool StartLocalReorder();

 private:
  static constexpr int kLocalReorderWindowSize = 3;
  static constexpr int kMaxLocalReorderIterations = 6;
  static constexpr int kMaxSwapCandidatesPerRowPair = 1;
  static constexpr int kMaxOptimalRegionRowsPerComponent = 4;
  static constexpr int kMaxOptimalRegionRowsPerStripe = 2;
  static constexpr int kMaxOptimalRegionCandidatesPerRow = 2;
  static constexpr int kMaxEjectionComponentsPerTarget = 2;
  static constexpr int kMaxEjectionDestinationRows = 1;
  static constexpr int kMaxCycleReceiverRows = 2;
  static constexpr int kMaxCycleComponentsPerReceiver = 2;
  static constexpr int kMaxFinalClusteringPasses = 6;
  static constexpr double kMinClusteringRelativeImprovement = 0.0001;
  static constexpr double kMinSignificantHpwlImprovement = 1e-9;

  struct SwapStats {
    int candidates = 0;
    int accepted = 0;
  };

  struct MoveStats {
    int candidates = 0;
    int source_singleton = 0;
    int width_blocked = 0;
    int p_well_blocked = 0;
    int n_well_blocked = 0;
    int evaluated = 0;
    int no_hpwl_improvement = 0;
    int accepted = 0;
    int ejection_attempts = 0;
    int ejection_evaluated = 0;
    int ejection_no_hpwl_improvement = 0;
    int ejection_accepted = 0;
    int cycle_attempts = 0;
    int cycle_evaluated = 0;
    int cycle_no_hpwl_improvement = 0;
    int cycle_accepted = 0;

    /** Accumulate counters from another relocation traversal. */
    void Add(const MoveStats& other);
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

  struct CandidateRow {
    GriddedRow* row = nullptr;
    double distance = 0;
  };

  struct RowRequirements {
    int used_width = 0;
    int p_well_height = 0;
    int n_well_height = 0;
  };

  struct DisplacementCandidate {
    Component* component = nullptr;
    OptimalRegion region;
    double current_distance = 0;
  };

  struct ClosedCycleCandidate {
    Component* displaced_component = nullptr;
    OptimalRegion displaced_region;
    GriddedRow* receiver_row = nullptr;
    Component* returning_component = nullptr;
    OptimalRegion returning_region;
    double hpwl_improvement = 0;
  };

  struct ClusterStats {
    int visited_rows = 0;
    int changed_rows = 0;
    int accepted_rows = 0;

    /** Accumulate counters from another row-clustering pass. */
    void Add(const ClusterStats& other);
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
  /** Compute row demand after optionally removing and adding a component. */
  RowRequirements ComputeRowRequirementsAfterAssignment(GriddedRow* row,
                                                        Component* removed,
                                                        Component* added) const;
  bool IsNonHeightIncreasingSwap(GriddedRow* first_row,
                                 Component* first_component,
                                 GriddedRow* second_row,
                                 Component* second_component) const;
  /** Collect the sorted union of nets incident to the supplied rows. */
  std::vector<int> CollectRowNetIds(const std::vector<GriddedRow*>& rows) const;
  /** Compute weighted HPWL for sorted unique net identifiers. */
  double NetWireLengthCost(const std::vector<int>& net_ids) const;
  double DistanceToOptimalRegionX(Component* component,
                                  const OptimalRegion& region) const;
  double DistanceToOptimalRegionY(GriddedRow* row, Component* component,
                                  const OptimalRegion& region) const;
  /** Return current Manhattan distance to an optimal region in microns. */
  double PhysicalDistanceToOptimalRegion(GriddedRow* row, Component* component,
                                         const OptimalRegion& region) const;
  /** Estimate the closest legal X distance from a row to an optimal region. */
  double DistanceFromRowToOptimalRegionX(GriddedRow* row, Component* component,
                                         const OptimalRegion& region) const;
  /** Return closest row-to-optimal-region distance in physical units. */
  double PhysicalDistanceFromRowToOptimalRegion(
      GriddedRow* row, Component* component, const OptimalRegion& region) const;
  /** Return candidate rows closer to a component's optimal region. */
  std::vector<CandidateRow> FindCandidateRows(
      GriddedRow* source_row, Component* component,
      const OptimalRegion& region) const;
  /** Find legal receiving rows for a component displaced by an ejection. */
  std::vector<CandidateRow> FindEjectionDestinationRows(
      GriddedRow* source_row, GriddedRow* target_row, Component* component,
      const OptimalRegion& region) const;
  /** Return target-row cells whose removal makes an incoming cell legal. */
  std::vector<DisplacementCandidate> FindDisplacementCandidates(
      GriddedRow* target_row, Component* incoming) const;
  /** Return the nearest legal target X inside a row and optimal region. */
  double ComputeMoveTargetX(GriddedRow* target_row, Component* component,
                            const OptimalRegion& region) const;
  /** Return the minimizer interval for weighted absolute-distance bounds. */
  static std::pair<double, double> ComputeWeightedMedianInterval(
      std::vector<std::pair<double, double>> weighted_bounds);
  OptimalRegion ComputeOptimalRegion(Component* component) const;
  void PlaceComponentInRow(GriddedRow* row, Component* component) const;
  /** Recompute component Y/orientation and legalize X in two changed rows. */
  void LegalizeRowsAfterAssignment(GriddedRow* first_row,
                                   GriddedRow* second_row);
  /** Recompute used width from row margins and assigned ordinary components. */
  void SynchronizeRowUsedSize(GriddedRow* row) const;
  /** Transfer a component's original-location record between row owners. */
  void TransferInitialLocation(GriddedRow* source_row, GriddedRow* target_row,
                               Component* component) const;
  bool TrySwap(GriddedRow* first_row, int first_index, GriddedRow* second_row,
               int second_index);
  /** Trial-move one component and commit only a legal exact-HPWL improvement.
   */
  bool TryMove(GriddedRow* source_row, Component* component,
               GriddedRow* target_row, double target_lx, MoveStats* stats);
  /**
   * Trial a three-row move that frees target-row width by displacing one cell.
   *
   * The source component enters its desired target row while one target-row
   * component moves to a legal receiver. The complete transaction is accepted
   * only when fixed row heights remain sufficient and exact affected-net HPWL
   * decreases.
   */
  bool TryEjectionChain(GriddedRow* source_row, Component* component,
                        GriddedRow* target_row,
                        const OptimalRegion& source_region, MoveStats* stats);
  /**
   * Trial a closed three-row assignment cycle without requiring free capacity.
   *
   * The source cell enters a full target row, one target resident enters a
   * receiver row, and one receiver resident returns to the newly freed source
   * row. The cycle is committed only when all fixed row capacities remain
   * legal and exact affected-net HPWL decreases.
   */
  bool TryClosedAssignmentCycle(GriddedRow* source_row, Component* component,
                                GriddedRow* target_row,
                                const OptimalRegion& source_region,
                                MoveStats* stats);
  /** Apply one previously validated closed three-row cycle candidate. */
  bool ApplyClosedAssignmentCycle(GriddedRow* source_row, Component* component,
                                  GriddedRow* target_row,
                                  const OptimalRegion& source_region,
                                  const ClosedCycleCandidate& candidate);
  SwapStats TryClosestComponentSwaps(GriddedRow* first_row,
                                     GriddedRow* second_row,
                                     int max_candidates);
  SwapStats TryOptimalRegionSwaps(GriddedRow* source_row, int source_index);
  MoveStats TryOptimalRegionMove(GriddedRow* source_row, Component* component,
                                 bool enable_ejection);
  /** Run relocation, optionally including the more expensive ejection search.
   */
  MoveStats RunRelocationStage(bool enable_ejection);
  /**
   * Project one row toward per-component optimal X regions without reordering.
   *
   * The projection uses isotonic regression after subtracting cumulative cell
   * widths, which guarantees legal non-overlap inside the row's reserved
   * ordinary-cell interval. The proposed row is retained only when exact HPWL
   * over all incident nets decreases.
   */
  bool ClusterRowX(GriddedRow* row, bool* changed);
  /** Apply one order-preserving X-clustering pass to every nonempty row. */
  ClusterStats RunSingleSegmentClustering();
  /** Log one exact single-segment clustering pass. */
  void LogClusteringPass(const std::string& stage_name,
                         const ClusterStats& stats, double hpwl_before);
  /** Log relocation acceptance and overlapping feasibility blockers. */
  void LogMoveStage(const MoveStats& stats, double hpwl_before);
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
  std::unordered_map<Component*, GriddedRow*> component_rows_;
  SnapshotCallback snapshot_callback_;
  int max_rounds_ = 6;
  double min_relative_improvement_ = 0.005;
  bool enable_vertical_swap_ = true;
  bool enable_relocation_ = false;
  int max_candidate_rows_ = kMaxOptimalRegionRowsPerComponent;
  size_t net_ignore_threshold_ = 100;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_DETAILED_PLACER_H_
