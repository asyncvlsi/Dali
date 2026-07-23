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
#ifndef DALI_PLACER_LEGALIZER_EXTENDED_TETRIS_LEGALIZER_H_
#define DALI_PLACER_LEGALIZER_EXTENDED_TETRIS_LEGALIZER_H_

#include "dali/circuit/component.h"
#include "dali/common/misc.h"
#include "dali/placer/displacement_viewer.h"
#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/gridded_row_legalizer.h"

namespace dali {

/** Extended row-based local legalizer with well-aware row assignment support.
 */
class ExtendedTetrisLegalizer : public Placer {
  friend class Dali;

 public:
  ExtendedTetrisLegalizer();

  /** Set row height in Dali grid units. */
  void SetRowHeight(int row_height);

  /** Set maximum local legalization iterations. */
  void SetMaxIteration(size_t max_iter);

  /** Set displacement cost factors for width and height. */
  void SetWidthHeightFactor(double k_width, double k_height);

  /** Set left-bound search factors for iterative legalization. */
  void SetLeftBoundFactor(double k_left, double k_left_step);

  /** Initialize row data from a gridded-row legalizer. */
  void InitializeFromGriddedRowLegalizer(GriddedRowLegalizer* grlg);

  /** Derive row information from the input circuit. */
  void SetRowInfoAuto();

  /** Detect legal row whitespace. */
  void DetectWhiteSpace();

  /** Initialize component id/location list. */
  void InitIndexLocList();

  /** Initialize all local legalizer state. */
  void InitLegalizer();

  int RowHeight() const;
  /**
   * Row-index geometry helpers. Rows are a fixed pitch here, so a Y location
   * maps to a row index and back: StartRow/EndRow/MaxRow give the index range,
   * HeightToRow the rows a cell of some height spans, LocToRow/RowToLoc convert
   * between a Y and its row, and AlignLocToRowLoc snaps a Y onto a row.
   */
  int StartRow(int y_loc) const;
  /** Row index whose top is at or above `y_loc`. */
  int EndRow(int y_loc) const;
  /** Highest row index a cell of `height` may start in and still fit. */
  int MaxRow(int height) const;
  /** Number of rows a cell of `height` spans. */
  int HeightToRow(int height) const;
  /** Row index containing `y_loc`. */
  int LocToRow(int y_loc) const;
  /** Y location of row `row_num`, offset by `displacement` rows. */
  int RowToLoc(int row_num, int displacement = 0) const;
  /** Snap a Y location down onto its row's origin. */
  int AlignLocToRowLoc(double y_loc) const;
  /** Whether a segment of space is legal to place into (wide enough, unblocked). */
  bool IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row) const;

  /** Whether a component fits within a single row's height. */
  bool IsFitToRow(int row_id, Component& component) const;
  /** Whether a component should be N-oriented in the row it lands in. */
  bool ShouldOrientN(int row_id, Component& component) const;

  void InitComponentContourForward();
  void InitAndSortComponentAscendingX();
  /** Consume row space for a component during the leftward legalization scan. */
  void UseSpaceLeft(Component const& component);
  /** Whether a component's current location is legal for the leftward scan. */
  bool IsCurrentLocLegalLeft(Value2D<int>& loc, Component& component);
  /** Leftmost X the leftward scan may reach within the given row range. */
  int WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row);
  /**
   * Nearest legal location at or left of the target.
   * @param loc receives the location. @return true if one was found.
   */
  bool FindLocLeft(Value2D<int>& loc, Component& component);
  bool LocalLegalizationLeft();

  void InitComponentContourBackward();
  void InitAndSortComponentDescendingX();
  /** Consume row space for a component during the rightward legalization scan. */
  void UseSpaceRight(Component const& component);
  /** Whether a component's current location is legal for the rightward scan. */
  bool IsCurrentLocLegalRight(Value2D<int>& loc, Component& component);
  /** Rightmost X the rightward scan may reach within the given row range. */
  int WhiteSpaceBoundRight(int lo_x, int hi_x, int lo_row, int hi_row);
  /**
   * Nearest legal location at or right of the target.
   * @param loc receives the location. @return true if one was found.
   */
  bool FindLocRight(Value2D<int>& loc, Component& component);
  bool LocalLegalizationRight();

  void ResetLeftLimitFactor();
  void UpdateLeftLimitFactor();
  /** Estimated HPWL of the current placement, for comparing legalization runs. */
  double EstimatedHPWL(Component& component, int x, int y);

  void ExportRowsToCircuit();
  bool StartPlacement() override;

  bool StartRowAssignment();

  /** Build the per-row available-space structure the scan primitives read. */
  void GenAvailSpace(std::string const& name_of_file = "avail_space.txt");

 protected:
  bool is_row_assignment_ = false;
  std::vector<std::vector<SegI>> rows_;
  std::vector<int> component_contour_;
  std::vector<ComponentInitialLocation> component_initial_locations_;

  int row_height_;
  bool row_height_set_;

  bool is_first_row_N_;

  bool legalize_from_left_;

  bool disable_cell_flip_ = false;
  int logged_legalization_failure_examples_ = 0;

  size_t cur_iter_;
  size_t max_iter_;

  double k_width_ = 0.0;
  double k_height_ = 0.0;

  double k_left_init_ = 0.5;
  double k_left_ = 0.5;
  double k_left_step_ = 0.5;
  double k_start = 2;  // 4
  double k_end = 3;    // 5

  // cached data
  int tot_num_rows_;

  // dump result
  bool is_dump = false;
  int dump_count = 0;
  double step_ratio = 0.1;
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_EXTENDED_TETRIS_LEGALIZER_H_
