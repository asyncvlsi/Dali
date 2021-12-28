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
#ifndef DALI_DALI_PLACER_WELLLEGALIZER_GRIDDEDROW_H_
#define DALI_DALI_PLACER_WELLLEGALIZER_GRIDDEDROW_H_

#include "dali/circuit/block.h"
#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/blocksegment.h"
#include "dali/placer/well_legalizer/rowsegment.h"

namespace dali {

struct BlockRegion {
  BlockRegion(Block *blk, int id) : p_blk(blk), region_id(id) {}
  Block *p_blk = nullptr;
  int region_id = 0;
};

class GriddedRow {
  friend class Stripe;
 public:
  GriddedRow() = default;

  bool IsOrientN() const;
  int UsedSize() const;
  void SetUsedSize(int used_size);
  void UseSpace(int width);

  void SetLLX(int lx);
  void SetURX(int ux);
  int LLX() const;
  int URX() const;
  double CenterX() const;

  void SetWidth(int width);
  int Width() const;

  void SetLLY(int ly);
  void SetURY(int uy);
  int LLY() const;
  int URY() const;
  double CenterY() const;

  void SetHeight(int height);
  void UpdateWellHeightUpward(int p_well_height, int n_well_height);
  void UpdateWellHeightDownward(int p_well_height, int n_well_height);
  int Height() const;
  int PHeight() const;
  int NHeight() const;
  int PNEdge() const;

  void SetLoc(int lx, int ly);

  void AddBlock(Block *blk_ptr);
  std::vector<Block *> &Blocks();
  std::vector<double2d> &InitLocations();
  void ShiftBlockX(int x_disp);
  void ShiftBlockY(int y_disp);
  void ShiftBlock(int x_disp, int y_disp);
  void UpdateBlockLocY();
  void LegalizeCompactX(int left);
  void LegalizeCompactX();
  void LegalizeLooseX(int space_to_well_tap = 0);
  void SetOrient(bool is_orient_N);
  void InsertWellTapCell(Block &tap_cell, int loc);

  void UpdateBlockLocationCompact();

  void MinDisplacementLegalization();
  void UpdateMinDisplacementLLY();
  double MinDisplacementLLY() const;

  std::vector<RowSegment> &Segments();
  void UpdateSegments(std::vector<SegI> &external_blockage);
  bool IsBelowMiddleLine(Block *p_blk) const;
  bool IsBelowTopPlusKFirstRegionHeight(Block *p_blk, int iteration) const;
  bool IsAboveMiddleLine(Block *p_blk) const;
  bool IsAboveBottomMinusKFirstRegionHeight(Block *p_blk, int iteration) const;
  bool IsOverlap(Block *p_blk, int iteration, bool is_upward) const;

  bool IsOrientMatching(Block *p_blk, int region_id) const;
  void AddBlockRegion(Block *p_blk, int region_id, bool is_upward);
  bool AttemptToAdd(Block *p_blk, bool is_upward = true);
  BlockOrient ComputeBlockOrient(Block *p_blk, bool is_upward) const;
  void LegalizeSegmentsX();
  void LegalizeSegmentsY();
  void RecomputeHeight(int p_well_height, int n_well_height);
  void InitializeBlockStretching();

  size_t AddWellTapCells(
      Circuit *p_ckt,
      BlockType *well_tap_type_ptr,
      size_t start_id,
      std::vector<SegI> &well_tap_cell_locs
  );

 private:
  bool is_orient_N_ = true; // orientation of this cluster
  std::vector<Block *> blk_list_; // list of blocks in this cluster
  std::vector<double2d> blk_initial_location_;

  /**** number of tap cells needed, and pointers to tap cells ****/
  int tap_cell_num_ = 0;
  Block *tap_cell_;

  /**** x/y coordinates and dimension ****/
  int lx_;
  int ly_;
  int width_;

  /**** total width of cells in this cluster, including reserved space for tap cells ****/
  int used_size_ = 0;

  /**** maximum p-well height and n-well height ****/
  int p_well_height_ = 0;
  int n_well_height_ = 0;
  int height_ = 0;

  /**** lly which gives minimal displacement ****/
  double min_displacement_lly_ = -DBL_MAX;

  /**** for multi-well legalization ****/
  std::vector<BlockRegion> blk_regions_;
  std::vector<RowSegment> segments_;
};

class ClusterSegment {
 private:
  int ly_;
  int height_;
 public:
  ClusterSegment(GriddedRow *cluster_ptr, int loc)
      : ly_(loc), height_(cluster_ptr->Height()) {
    cluster_list.push_back(cluster_ptr);
  }
  std::vector<GriddedRow *> cluster_list;

  int LY() const { return ly_; }
  int UY() const { return ly_ + height_; }
  int Height() const { return height_; }

  bool IsNotOnBottom(ClusterSegment &sc) const {
    return sc.LY() < UY();
  }
  void Merge(ClusterSegment &sc, int lower_bound, int upper_bound);
  void UpdateClusterLocation();
};

}

#endif //DALI_DALI_PLACER_WELLLEGALIZER_GRIDDEDROW_H_
