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
#ifndef DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_
#define DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_

#include <unordered_map>
#include <vector>

#include "dali/circuit/block.h"
#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/blockhelper.h"
#include "dali/placer/well_legalizer/optimizationhelper.h"

namespace dali {

class RowSegment {
 public:
  RowSegment() = default;

  void SetLLX(int lx);
  void SetURX(int ux);
  void SetWidth(int width);
  void SetUsedSize(int used_size);

  int LLX() const;
  int URX() const;
  int Width() const;
  int UsedSize() const;

  std::vector<BlockRegion> &BlkRegions();
  void AddBlockRegion(Block *blk_ptr, int region_id);
  void RemoveBlockRegion(Block *blk_ptr, int region_id);
  void MinDisplacementLegalization(bool use_init_loc);
  void SnapCellToPlacementGrid();

  void SetOptimalAnchorWeight(double weight);
  void FitInRange(std::vector<BlkDispVar> &vars);
  double DispCost(
      std::vector<BlkDispVar> &vars,
      int l, int r,
      bool is_linear
  );
  void FindBestLocalOrder(
      std::vector<BlkDispVar> &res,
      double &best_cost,
      std::vector<BlkDispVar> &vars,
      int cur, int l, int r,
      double left_bound, double right_bound,
      double gap, int range,
      bool is_linear
  );
  void LocalReorder(
      std::vector<BlkDispVar> &vars,
      int range = 3,
      int omit = 0,
      bool is_linear = false
  );
  void LocalReorder2(
      std::vector<BlkDispVar> &vars
  );
  std::vector<BlkDispVar> OptimizeQuadraticDisplacement(
      double lambda,
      bool is_weighted_anchor,
      bool is_reorder
  );
  std::vector<BlkDispVar> OptimizeLinearDisplacement(
      double lambda,
      bool is_weighted_anchor,
      bool is_reorder
  );

  void GenSubCellTable(
      std::ofstream &ost_cluster,
      std::ofstream &ost_sub_cell,
      std::ofstream &ost_discrepancy,
      std::ofstream &ost_displacement,
      double row_ly,
      double row_uy
  );
 private:
  // list of blocks in this segment
  std::vector<BlockRegion> blk_regions_;
  int lx_ = INT_MIN;
  int width_ = 0;
  int used_size_ = 0;

  /**** for iterative displacement optimization ****/
  double opt_anchor_weight_ = 0;
};

}

#endif //DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_
