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

  void SetLLX(int32_t lx);
  void SetURX(int32_t ux);
  void SetWidth(int32_t width);
  void SetUsedSize(int32_t used_size);

  int32_t LLX() const;
  int32_t URX() const;
  int32_t Width() const;
  int32_t UsedSize() const;

  std::vector<BlockRegion> &BlkRegions();
  void AddBlockRegion(Block *blk_ptr, int32_t region_id);
  void MinDisplacementLegalization(bool use_init_loc);
  void SnapCellToPlacementGrid();

  void SetOptimalAnchorWeight(double weight);
  void FitInRange(std::vector<BlkDispVar> &vars);
  double DispCost(
      std::vector<BlkDispVar> &vars,
      int32_t l, int32_t r,
      bool is_linear
  );
  void FindBestLocalOrder(
      std::vector<BlkDispVar> &res,
      double &best_cost,
      std::vector<BlkDispVar> &vars,
      int32_t cur, int32_t l, int32_t r,
      double left_bound, double right_bound,
      double gap, int32_t range,
      bool is_linear
  );
  void LocalReorder(
      std::vector<BlkDispVar> &vars,
      int32_t range = 3,
      int32_t omit = 0,
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
  int32_t lx_ = INT_MIN;
  int32_t width_ = 0;
  int32_t used_size_ = 0;

  /**** for iterative displacement optimization ****/
  double opt_anchor_weight_ = 0;
};

}

#endif //DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_
