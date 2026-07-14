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
#ifndef DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_
#define DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_

#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "dali/circuit/component.h"
#include "dali/circuit/row.h"
#include "dali/placer/placer.h"

namespace dali {

/**
 * Wirelength-driven detailed placer for already legalized standard-cell rows.
 *
 * This starts with the local re-ordering technique from the ISPD 2005 detailed
 * placement paper: for each small window of consecutive cells in a row segment,
 * enumerate all left-to-right orders and keep the best HPWL-improving order.
 */
class DetailedPlacer : public Placer {
 public:
  /** Callback used by the application to emit visualization snapshots. */
  using SnapshotCallback =
      std::function<void(const std::string& id, const std::string& label,
                         const std::string& subgroup, int iteration)>;

  /** Set a callback invoked after each detailed-placement optimization phase.
   */
  void SetSnapshotCallback(SnapshotCallback snapshot_callback);

  /** Set the maximum number of detailed-placement optimization rounds. */
  void SetMaxOptimizationRounds(int max_optimization_rounds);

  /** Set how many optimal-region move candidates are evaluated per round. */
  void SetMaxMoveCandidatesPerRound(int max_move_candidates_per_round);

  /** Set the pin-count cutoff for nets omitted from move proposals and costs.
   */
  void SetNetIgnoreThreshold(int net_ignore_threshold);

  bool StartPlacement() override;

 private:
  static constexpr int kLocalReorderWindowSize = 3;
  static constexpr int kMaxOptimalRegionRows = 4;
  static constexpr int kMaxCandidatesPerRow = 3;
  static constexpr int kMaxSegmentsPerRow = 3;
  static constexpr int kDefaultMaxMoveCandidatesPerRound = 10000;
  static constexpr int kDefaultMaxOptimizationRounds = 5;
  static constexpr double kMinRelativeRoundImprovement = 0.001;

  struct OptimalRegion {
    bool valid = false;
    double lx = 0;
    double ly = 0;
    double ux = 0;
    double uy = 0;
  };

  double WindowWireLengthCost(const std::vector<Component*>& components,
                              int start, int window_size);
  void PlaceWindow(const std::vector<Component*>& order, int left_bound,
                   int right_bound);
  bool ReorderWindow(std::vector<Component*>* components, int start,
                     int window_size);
  int LocalReorderSegment(GeneralRowSegment* segment, int window_size);

  /** Index each legalized component by its current row and free segment. */
  void BuildSwapIndex();

  /** Compute the HPWL-optimal rectangle induced by a component's other pins. */
  OptimalRegion ComputeOptimalRegion(Component* component) const;

  /**
   * Return physical HPWL for one net using unflipped macro pin offsets.
   *
   * Bookshelf's official evaluator combines lower-left cell locations with the
   * N-orientation pin offsets from the `.nets` file and ignores row
   * orientation. The standard-cell detailed placer uses this cost so local
   * accept/reject decisions match the benchmark objective.
   */
  double BookshelfStyleWireLength(int net_id) const;

  /** Return Manhattan distance from a location to an optimal region. */
  double DistanceToRegion(double x, double y,
                          const OptimalRegion& region) const;

  /** Return the row indices nearest to an optimal region's Y interval. */
  std::vector<int> FindClosestRows(const OptimalRegion& region) const;

  /** Return physical HPWL for a deduplicated set of affected nets. */
  double AffectedWireLength(const std::set<int>& net_ids) const;

  /** Repack one segment with Abacus while preserving its legal boundaries. */
  bool LegalizeSegment(GeneralRowSegment* segment, GeneralRow* row);

  /** Return the total component width currently assigned to a segment. */
  int UsedWidth(GeneralRowSegment* segment) const;

  /** Return row index for a row pointer owned by the circuit row vector. */
  size_t RowIndex(GeneralRow* row) const;

  /** Return the row segments nearest to an x location inside one row. */
  std::vector<GeneralRowSegment*> FindClosestSegmentsInRow(
      int row_index, double target_x) const;

  /**
   * Trial-move a component into a free segment and commit only legal
   * HPWL-improving results.
   *
   * The source and destination segments are repacked transactionally. The
   * accepted move may shift other cells in both segments, so the HPWL check
   * includes every net incident to those affected segments.
   */
  bool TryMove(Component* component, GeneralRow* target_row,
               GeneralRowSegment* target_segment, double target_lx);

  /** Move cells into whitespace near their optimal regions. */
  int RunOptimalRegionMoves();

  /**
   * Quickly reject a swap unless exchanging the two raw locations improves
   * the physical HPWL of the pair's incident nets.
   */
  bool IsPromisingSwap(Component* first, Component* second,
                       GeneralRow* first_row, GeneralRow* second_row) const;

  /**
   * Trial-swap two components and commit only an HPWL-improving legal result.
   *
   * Unequal-width cells are supported because both affected segments are
   * repacked. The cost transaction includes every net of every cell whose
   * location changes during that repacking.
   */
  bool TrySwap(Component* first, Component* second);

  /** Visit cells and try nearby candidates in their optimal regions. */
  int RunOptimalRegionSwaps();

  /**
   * Repack each segment toward per-cell optimal-region targets.
   *
   * Cell order and segment legality are preserved, and a clustered placement
   * is committed only when all nets incident to that segment improve.
   */
  int RunSingleSegmentClustering();

  /** Run local reordering across every legal row segment once. */
  int RunLocalReordering(int* visited_segment_count);

  /** Publish a detailed-placement checkpoint if visualization is enabled. */
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& subgroup, int iteration);

  std::vector<std::vector<Component*>> row_components_;
  std::vector<std::vector<GeneralRowSegment*>> row_segments_;
  std::vector<GeneralRow*> component_rows_;
  std::vector<GeneralRowSegment*> component_segments_;
  SnapshotCallback snapshot_callback_;
  int max_move_candidates_per_round_ = kDefaultMaxMoveCandidatesPerRound;
  int max_optimization_rounds_ = kDefaultMaxOptimizationRounds;
  size_t net_ignore_threshold_ = 100;
};

}  // namespace dali

#endif  // DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_
