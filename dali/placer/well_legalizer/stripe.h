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
#ifndef DALI_PLACER_WELLLEGALIZER_STRIPE_H_
#define DALI_PLACER_WELLLEGALIZER_STRIPE_H_

#include "dali/circuit/block.h"
#include "dali/circuit/circuit.h"
#include "dali/common/config.h"
#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/griddedrow.h"

#if DALI_USE_CPLEX
#include <ilcplex/ilocplex.h>
ILOSTLBEGIN
#endif

namespace dali {

class Stripe {
 public:
  int32_t lx_;
  int32_t ly_;
  int32_t width_;
  int32_t height_;
  int32_t max_blk_capacity_per_cluster_;

  int32_t contour_;
  int32_t used_height_;
  int32_t cluster_count_;
  GriddedRow *front_row_;
  int32_t front_id_;
  std::vector<GriddedRow> gridded_rows_;
  bool is_bottom_up_ = false;

  int32_t block_count_;
  std::vector<Block *> blk_ptrs_vec_;
  std::unordered_map<Block *, int32_t> blk_ptr_2_row_id_;

  bool is_first_row_orient_N_ = true;
  std::vector<RectI> well_rect_list_;

  bool is_checkerboard_mode_ = false;
  int32_t well_tap_cell_width_ = -1;
  std::vector<SegI> well_tap_cell_location_even_;
  std::vector<SegI> well_tap_cell_location_odd_;

  std::vector<RowSegment *> row_seg_ptrs_;

  std::vector<double> displacements_;
  std::vector<double> discrepancies_;
  double max_discrepancy_ = 0;

  double max_disp_ = 0;

  int32_t LLX() const { return lx_; }
  int32_t LLY() const { return ly_; }
  int32_t URX() const { return lx_ + width_; }
  int32_t URY() const { return ly_ + height_; }
  int32_t Width() const { return width_; }
  int32_t Height() const { return height_; }

  bool HasNoRowsSpillingOut() const;

  void MinDisplacementAdjustment();

  void SortBlocksBasedOnLLY();
  void SortBlocksBasedOnURY();
  void SortBlocksBasedOnStretchedURY();
  void SortBlocksBasedOnYLocation(int32_t criterion);

  void PrecomputeWellTapCellLocation(
      bool is_checker_board_mode,
      int32_t tap_cell_interval_grid,
      BlockType *well_tap_type_ptr
  );

  void UpdateFrontClusterUpward(int32_t p_height, int32_t n_height);
  void SimplyAddFollowingClusters(Block *p_blk, bool is_upward);
  bool AddBlockToFrontCluster(Block *p_blk, bool is_upward);
  bool AddBlockToFrontClusterWithDispCheck(
      Block *p_blk,
      double displacement_upper_limit,
      bool is_upward
  );
  size_t FitBlocksToFrontSpaceUpward(size_t start_id, int32_t current_iteration);
  size_t FitBlocksToFrontSpaceUpwardWithDispCheck(
      size_t start_id, double displacement_upper_limit
  );
  void LegalizeFrontCluster(bool use_init_loc);
  void UpdateRemainingClusters(int32_t p_height, int32_t n_height, bool is_upward);
  void UpdateBlockStretchLength();

  void UpdateFrontClusterDownward(int32_t p_height, int32_t n_height);
  size_t FitBlocksToFrontSpaceDownward(size_t start_id, int32_t current_iteration);

  void UpdateBlockYLocation();
  void CleanUpTemporaryRowSegments();

  size_t AddWellTapCells(
      Circuit *p_ckt,
      BlockType *well_tap_type_ptr,
      size_t start_id
  );

  bool IsLeftmostPlacementLegal();
  bool IsStripeLegal();

  void CollectAllRowSegments();
  void UpdateSubCellLocs(std::vector<BlkDispVar> &vars);
  void OptimizeDisplacementInEachRowSegment(
      double lambda,
      bool is_weighted_anchor,
      bool is_reorder
  );
  void ComputeAverageLoc();
  void ReportIterativeStatus(int32_t i);
  bool IsDiscrepancyConverge();
  void SetBlockLoc();
  void ClearMultiRowCellBreaking();
  void IterativeCellReordering(int32_t max_iter, int32_t number_of_threads = 1);

  void SortBlocksInEachRow();

  size_t OutOfBoundCell();

#if DALI_USE_CPLEX
  std::unordered_map<Block *, IloInt> blk_ptr_2_tmp_id;
  std::unordered_map<IloInt, Block *> blk_tmp_id_2_ptr;
  void PopulateVariableArray(IloModel &model, IloNumVarArray &x);
  void AddVariableConstraints(
      IloModel &model, IloNumVarArray &x, IloRangeArray &c
  );
  void ConstructQuadraticObjective(IloModel &model, IloNumVarArray &x);
  void CreateQPModel(IloModel &model, IloNumVarArray &x, IloRangeArray &c);
  bool SolveQPProblem(IloCplex &cplex, IloNumVarArray &var);
  bool OptimizeDisplacementUsingQuadraticProgramming(int32_t number_of_threads = 1);
#endif

  /**** for standard cells ****/
  int32_t row_height_ = 1;
  void ImportStandardRowSegments(phydb::PhyDB &phydb, Circuit &ckt);
  int32_t LocY2RowId(double lly);
  double EstimateCost(int32_t row_id, Block *blk_ptr, SegI &range, double density);
  void AddBlockToRow(int32_t row_id, Block *blk_ptr, SegI range);
  void AssignStandardCellsToRowSegments(/*double white_space_usage*/);
};

struct ClusterStripe {
  int32_t lx_;
  int32_t width_;

  int32_t block_count_;
  std::vector<Block *> block_list_;

  std::vector<RectI> well_rect_list_;

  std::vector<std::vector<SegI>> white_space_; // white space in each row
  std::vector<Stripe> stripe_list_;

  int32_t Width() const { return width_; }
  int32_t LLX() const { return lx_; }
  int32_t URX() const { return lx_ + width_; }
  Stripe *GetStripeMatchSeg(SegI seg, int32_t y_loc);
  Stripe *GetStripeMatchBlk(Block *blk_ptr);
  Stripe *GetStripeClosestToBlk(Block *blk_ptr, double &distance);
  void AssignBlockToSimpleStripe();
};

}

#endif //DALI_PLACER_WELLLEGALIZER_STRIPE_H_
