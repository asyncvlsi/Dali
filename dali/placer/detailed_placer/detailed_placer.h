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
#include <optional>
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

  /** Restrict standard-cell swaps to exchanges that fit the existing slots. */
  void SetUseStandardCellSlotSwaps(bool enabled) {
    use_standard_cell_slot_swaps_ = enabled;
  }

  /** Set the pin-count cutoff for nets omitted from move proposals and costs.
   */
  void SetNetIgnoreThreshold(int net_ignore_threshold);

  /** Use a fixed DEF site pitch and score orientation-aware pin HPWL. */
  void SetFixedRowSiteWidth(int width) { fixed_row_site_width_ = width; }

  bool StartPlacement() override;

 private:
  int fixed_row_site_width_ = 0;

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

  struct BookshelfPin {
    const Component* component;
    double offset_x;
    double offset_y;
  };

  int SegmentSiteWidth() const;
  void BuildBookshelfPinIndex();
  double NetWireLength(int net_id) const;

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

  using NetWireLengths = std::vector<std::optional<double>>;

  /** Return physical HPWL for affected net IDs in ascending order. */
  template <typename NetIds>
  double AffectedWireLength(const NetIds& net_ids,
                            NetWireLengths* current_costs = nullptr) const;

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
   * Compare physical HPWL of the pair's incident nets before and after
   * exchanging locations. Restore both cells before returning.
   */
  bool SwapLowersHpwlBeforeLegalization(
      Component* first, Component* second, GeneralRow* first_row,
      GeneralRow* second_row, NetWireLengths& current_costs) const;

  struct SwapSlot {
    Component** segment_entry = nullptr;
    Component** row_entry = nullptr;
    double width = 0.0;
  };

  struct RepackedSwapSnapshot {
    struct Placement {
      Component* component;
      double lx;
      double ly;
      ComponentOrient orientation;
    };

    std::vector<Component*> first_components;
    std::vector<Component*> second_components;
    std::vector<Placement> placements;

    void Capture(GeneralRowSegment* first, GeneralRowSegment* second);
    void Restore(GeneralRowSegment* first, GeneralRowSegment* second) const;
  };

  struct RepackedSwapScratch {
    std::vector<int> affected_nets;
    std::vector<unsigned char> seen_nets;
    RepackedSwapSnapshot original;
    RepackedSwapSnapshot candidate;
  };

  std::vector<SwapSlot> BuildSwapSlots();
  bool TrySlotSwap(Component* first, Component* second,
                   std::vector<SwapSlot>& slots, NetWireLengths& current_costs);
  bool TryRepackedSwap(Component* first, Component* second,
                       NetWireLengths& current_costs,
                       RepackedSwapScratch& scratch);

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
  std::vector<BookshelfPin> bookshelf_pins_;
  std::vector<std::size_t> bookshelf_net_starts_;
  const Net* bookshelf_nets_ = nullptr;
  double bookshelf_grid_x_ = 0.0;
  double bookshelf_grid_y_ = 0.0;
  SnapshotCallback snapshot_callback_;
  int max_move_candidates_per_round_ = kDefaultMaxMoveCandidatesPerRound;
  int max_optimization_rounds_ = kDefaultMaxOptimizationRounds;
  bool use_standard_cell_slot_swaps_ = false;
  size_t net_ignore_threshold_ = 100;
};

}  // namespace dali

#endif  // DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_
