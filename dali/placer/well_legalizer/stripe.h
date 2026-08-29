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
#ifndef DALI_PLACER_WELL_LEGALIZER_STRIPE_H_
#define DALI_PLACER_WELL_LEGALIZER_STRIPE_H_

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"
#include "dali/circuit/component.h"
#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/gridded_row.h"

namespace dali {

/** Vertical legalization stripe containing gridded rows and assigned
 * components. */
/**
 * One well-legalization region: a vertical slice of a stripe column.
 *
 * A stripe owns the gridded rows stacked inside it (`gridded_rows_`) and is the
 * unit that legalization succeeds or fails on. Rows grow from a contour that
 * advances as clusters are added, so `contour_` and `used_height_` track how
 * much of the stripe height has been consumed; `used_height_` exceeding
 * `Height()` is the signal that this stripe could not be legalized.
 *
 * Containment runs StripeColumn -> Stripe -> GriddedRow -> RowSegment ->
 * Component.
 */
class Stripe {
 public:
  int lx_;
  int ly_;
  int width_;
  int height_;
  int max_component_capacity_per_cluster_;

  int contour_;
  int used_height_;
  int cluster_count_;
  GriddedRow* front_row_;
  int front_id_;
  std::vector<GriddedRow> gridded_rows_;
  bool is_bottom_up_ = false;

  int component_count_;
  std::vector<Component*> component_ptrs_vec_;
  std::unordered_map<Component*, int> component_ptr_2_row_id_;

  bool is_first_row_orient_N_ = true;
  std::vector<RectI> well_rect_list_;

  bool is_checkerboard_mode_ = false;
  int well_tap_width_ = -1;
  std::vector<SegI> well_tap_cell_location_even_;
  std::vector<SegI> well_tap_cell_location_odd_;

  std::vector<RowSegment*> row_seg_ptrs_;

  std::vector<double> displacements_;
  std::vector<double> discrepancies_;
  double max_discrepancy_ = 0;

  double max_disp_ = 0;

  /** Return lower-left x in Dali grid units. */
  int LLX() const { return lx_; }

  /** Return lower-left y in Dali grid units. */
  int LLY() const { return ly_; }

  /** Return upper-right x in Dali grid units. */
  int URX() const { return lx_ + width_; }

  /** Return upper-right y in Dali grid units. */
  int URY() const { return ly_ + height_; }

  /** Return stripe width in Dali grid units. */
  int Width() const { return width_; }

  /** Return stripe height in Dali grid units. */
  int Height() const { return height_; }

  /** Return true when all rows remain inside the stripe boundary. */
  bool HasNoRowsSpillingOut() const;

  void MinDisplacementAdjustment();

  void SortComponentsBasedOnLLY();
  void SortComponentsBasedOnURY();
  void SortComponentsBasedOnStretchedURY();
  /** Sort the stripe's components by Y, the order clustering consumes them in. */
  void SortComponentsBasedOnYLocation(int criterion);

  void PrecomputeWellTapCellLocation(bool is_checker_board_mode,
                                     int tap_cell_interval_grid,
                                     Macro* well_tap_macro);

  /**
   * Cluster the stripe's components into gridded rows, growing from one end.
   * UpdateFrontCluster{Upward,Downward} advance the frontier row, AddComponent*
   * and LegalizeFrontCluster fill and legalize it, and UpdateRemainingClusters
   * carries the rest forward. The two directions build from bottom or from top.
   */
  void UpdateFrontClusterUpward(int p_height, int n_height);
  /** Append the remaining components as clusters without re-optimizing order. */
  void SimplyAddFollowingClusters(Component* component, bool is_upward);
  /** Add a component to the row currently being built. */
  bool AddComponentToFrontCluster(Component* component, bool is_upward);
  bool AddComponentToFrontClusterWithDispCheck(Component* component,
                                               double displacement_upper_limit,
                                               bool is_upward);
  size_t FitComponentsToFrontSpaceUpward(size_t start_id,
                                         int current_iteration);
  size_t FitComponentsToFrontSpaceUpwardWithDispCheck(
      size_t start_id, double displacement_upper_limit);
  /** Legalize the frontier row's cells once it is full. */
  void LegalizeFrontCluster(bool use_init_loc);
  /** Carry the not-yet-clustered components forward after a row closes. */
  void UpdateRemainingClusters(int p_height, int n_height, bool is_upward);
  void UpdateComponentStretchLength();

  /** Advance the frontier row downward (top-to-bottom clustering). */
  void UpdateFrontClusterDownward(int p_height, int n_height);
  size_t FitComponentsToFrontSpaceDownward(size_t start_id,
                                           int current_iteration);

  void UpdateComponentYLocation();
  void CleanUpTemporaryRowSegments();

  size_t AddWellTapCells(Circuit* p_ckt, Macro* well_tap_macro,
                         size_t start_id);

  bool IsLeftmostPlacementLegal();
  bool IsStripeLegal();

  void CollectAllRowSegments();
  /** Refresh per-cell locations after a clustering or reordering step. */
  void UpdateSubCellLocs(std::vector<ComponentDisplacementVariable>& vars);
  void OptimizeDisplacementInEachRowSegment(double lambda,
                                            bool is_weighted_anchor,
                                            bool is_reorder);
  void ComputeAverageLoc();
  /** Log progress of the iterative cell-reordering loop. */
  void ReportIterativeStatus(int i);
  bool IsDiscrepancyConverged();
  void SetComponentLoc();
  void ClearMultiRowCellBreaking();
  /** Improve intra-row cell order over several passes to cut displacement. */
  void IterativeCellReordering(int max_iter, int number_of_threads = 1);

  void SortComponentsInEachRow();

  size_t OutOfBoundCell();

  /**** for standard cells ****/
  int row_height_ = 1;
  /** Populate the stripe's rows from PhyDB standard-cell row definitions. */
  void ImportStandardRowSegments(phydb::PhyDB& phydb, Circuit& ckt);
  /** Map a Y location to the index of the gridded row containing it. */
  int LocY2RowId(double lly);
  double EstimateCost(int row_id, Component* component_ptr, SegI& range,
                      double density);
  /** Assign a component to a specific gridded row by index. */
  void AddComponentToRow(int row_id, Component* component_ptr, SegI range);
  /** Distribute standard cells across the row segments they legalize within. */
  void AssignStandardCellsToRowSegments(/*double white_space_usage*/);
};

/** Column-like collection of legalization stripes and their assigned
 * components. */
/**
 * A full-height column of the placement region, and its own well region.
 *
 * Column boundaries are chosen by the space partitioner so that neighbouring
 * columns are separated by `well_spacing`, which is what makes each column an
 * independent well region. A column is cut vertically into stripes, and those
 * stripes hold the gridded rows.
 *
 * Row heights are content-dependent, so the P/N boundaries of adjacent columns
 * do not line up; nothing here may assume a row grid shared across columns.
 */
struct StripeColumn {
  int lx_;
  int width_;

  int component_count_;
  std::vector<Component*> component_list_;

  std::vector<RectI> well_rect_list_;

  std::vector<std::vector<SegI>> white_space_;  // white space in each row
  std::vector<Stripe> stripe_list_;

  /** Return stripe-column width in Dali grid units. */
  int Width() const { return width_; }

  /** Return lower-left x in Dali grid units. */
  int LLX() const { return lx_; }

  /** Return upper-right x in Dali grid units. */
  int URX() const { return lx_ + width_; }
  /** The stripe a segment at `y_loc` belongs to, for cross-stripe matching. */
  Stripe* GetStripeMatchSeg(SegI seg, int y_loc);
  /** The stripe a component belongs to, for cross-stripe matching. */
  Stripe* GetStripeMatchComponent(Component* component_ptr);
  Stripe* GetStripeClosestToComponent(Component* component_ptr,
                                      double& distance);
  void AssignComponentToSimpleStripe();
  /**
   * Re-own this column's components by fragment capacity instead of proximity.
   *
   * Only for use after proximity ownership has produced a placement that failed
   * to cluster: it keeps every component whose fragment still has room exactly
   * where it was and moves only the overflow. Returns false, changing nothing,
   * when some component has no fragment with room anywhere.
   */
  bool AssignComponentToSimpleStripeByCapacity(
      const GriddedCapacityConfig& config, int* moved_component_count,
      int* overloaded_before, int* overloaded_after, std::string* refusal);
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_STRIPE_H_
