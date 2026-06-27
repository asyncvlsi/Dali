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
#include "dali/circuit/component.h"
#include "dali/common/config.h"
#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/gridded_row.h"

#if DALI_USE_CPLEX
#include <ilcplex/ilocplex.h>
ILOSTLBEGIN
#endif

namespace dali {

/** Vertical legalization stripe containing gridded rows and assigned
 * components. */
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
  int well_tap_cell_width_ = -1;
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
  void SortComponentsBasedOnYLocation(int criterion);

  void PrecomputeWellTapCellLocation(bool is_checker_board_mode,
                                     int tap_cell_interval_grid,
                                     Macro* well_tap_macro);

  void UpdateFrontClusterUpward(int p_height, int n_height);
  void SimplyAddFollowingClusters(Component* component, bool is_upward);
  bool AddComponentToFrontCluster(Component* component, bool is_upward);
  bool AddComponentToFrontClusterWithDispCheck(Component* component,
                                               double displacement_upper_limit,
                                               bool is_upward);
  size_t FitComponentsToFrontSpaceUpward(size_t start_id,
                                         int current_iteration);
  size_t FitComponentsToFrontSpaceUpwardWithDispCheck(
      size_t start_id, double displacement_upper_limit);
  void LegalizeFrontCluster(bool use_init_loc);
  void UpdateRemainingClusters(int p_height, int n_height, bool is_upward);
  void UpdateComponentStretchLength();

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
  void UpdateSubCellLocs(std::vector<ComponentDisplacementVariable>& vars);
  void OptimizeDisplacementInEachRowSegment(double lambda,
                                            bool is_weighted_anchor,
                                            bool is_reorder);
  void ComputeAverageLoc();
  void ReportIterativeStatus(int i);
  bool IsDiscrepancyConverged();
  void SetComponentLoc();
  void ClearMultiRowCellBreaking();
  void IterativeCellReordering(int max_iter, int number_of_threads = 1);

  void SortComponentsInEachRow();

  size_t OutOfBoundCell();

#if DALI_USE_CPLEX
  std::unordered_map<Component*, IloInt> component_ptr_2_tmp_id;
  std::unordered_map<IloInt, Component*> component_temp_id_to_ptr_;
  void PopulateVariableArray(IloModel& model, IloNumVarArray& x);
  void AddVariableConstraints(IloModel& model, IloNumVarArray& x,
                              IloRangeArray& c);
  void ConstructQuadraticObjective(IloModel& model, IloNumVarArray& x);
  void CreateQPModel(IloModel& model, IloNumVarArray& x, IloRangeArray& c);
  bool SolveQPProblem(IloCplex& cplex, IloNumVarArray& var);
  bool OptimizeDisplacementUsingQuadraticProgramming(int number_of_threads = 1);
#endif

  /**** for standard cells ****/
  int row_height_ = 1;
  void ImportStandardRowSegments(phydb::PhyDB& phydb, Circuit& ckt);
  int LocY2RowId(double lly);
  double EstimateCost(int row_id, Component* component_ptr, SegI& range,
                      double density);
  void AddComponentToRow(int row_id, Component* component_ptr, SegI range);
  void AssignStandardCellsToRowSegments(/*double white_space_usage*/);
};

/** Column-like collection of legalization stripes and their assigned
 * components. */
struct ClusterStripe {
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
  Stripe* GetStripeMatchSeg(SegI seg, int y_loc);
  Stripe* GetStripeMatchComponent(Component* component_ptr);
  Stripe* GetStripeClosestToComponent(Component* component_ptr,
                                      double& distance);
  void AssignComponentToSimpleStripe();
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_STRIPE_H_
