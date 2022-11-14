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
#ifndef DALI_PLACER_LEGALIZER_LGTETRISEX_H_
#define DALI_PLACER_LEGALIZER_LGTETRISEX_H_

#include "dali/circuit/block.h"
#include "dali/placer/displacement_viewer.h"
#include "dali/common/misc.h"
#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/griddedrowlegalizer.h"

namespace dali {

class LGTetrisEx : public Placer {
  friend class Dali;
 public:
  LGTetrisEx();

  void SetRowHeight(int row_height);
  void SetMaxIteration(size_t max_iter);
  void SetWidthHeightFactor(double k_width, double k_height);
  void SetLeftBoundFactor(double k_left, double k_left_step);

  void InitializeFromGriddedRowLegalizer(GriddedRowLegalizer *grlg);
  void SetRowInfoAuto();
  void DetectWhiteSpace();
  void InitIndexLocList();
  void InitLegalizer();

  int RowHeight() const;
  int StartRow(int y_loc) const;
  int EndRow(int y_loc) const;
  int MaxRow(int height) const;
  int HeightToRow(int height) const;
  int LocToRow(int y_loc) const;
  int RowToLoc(int row_num, int displacement = 0) const;
  int AlignLocToRowLoc(double y_loc) const;
  bool IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row) const;

  bool IsFitToRow(int row_id, Block &block) const;
  bool ShouldOrientN(int row_id, Block &block) const;

  void InitBlockContourForward();
  void InitAndSortBlockAscendingX();
  void UseSpaceLeft(Block const &block);
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, Block &block);
  int WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row);
  bool FindLocLeft(Value2D<int> &loc, Block &block);
  bool LocalLegalizationLeft();

  void InitBlockContourBackward();
  void InitAndSortBlockDescendingX();
  void UseSpaceRight(Block const &block);
  bool IsCurrentLocLegalRight(Value2D<int> &loc, Block &block);
  int WhiteSpaceBoundRight(int lo_x, int hi_x, int lo_row, int hi_row);
  bool FindLocRight(Value2D<int> &loc, Block &block);
  bool LocalLegalizationRight();

  void ResetLeftLimitFactor();
  void UpdateLeftLimitFactor();
  double EstimatedHPWL(Block &block, int x, int y);

  bool StartPlacement() override;

  void PlaceFillerCells();

  bool StartRowAssignment();

  void GenAvailSpace(std::string const &name_of_file = "avail_space.txt");
 protected:
  bool is_row_assignment_ = false;
  std::vector<std::vector<SegI>> row_segments_;
  std::vector<int> block_contour_;
  std::vector<BlkInitPair> blk_inits_;

  int row_height_;
  bool row_height_set_;

  bool is_first_row_N_;

  bool legalize_from_left_;

  size_t cur_iter_;
  size_t max_iter_;

  double k_width_ = 0.0;
  double k_height_ = 0.0;

  double k_left_init_ = 0.5;
  double k_left_ = 0.5;
  double k_left_step_ = 0.5;
  double k_start = 2; // 4
  double k_end = 3; // 5

  //cached data
  int tot_num_rows_;

  // dump result
  bool is_dump = false;
  int dump_count = 0;
  double step_ratio = 0.1;
};

}

#endif //DALI_PLACER_LEGALIZER_LGTETRISEX_H_
