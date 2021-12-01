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
#ifndef DALI_DALI_PLACER_WELLLEGALIZER_CLUSTER_H_
#define DALI_DALI_PLACER_WELLLEGALIZER_CLUSTER_H_

#include "blocksegment.h"
#include "dali/circuit/block.h"

namespace dali {

class Cluster {
 public:
  Cluster() = default;

  bool IsOrientN() const;
  bool IsSingle() const;
  int UsedSize() const;
  void SetUsedSize(int used_size);
  void UseSpace(int width);
  int UsedSize1() const;
  void SetUsedSize1(int used_size);
  void UseSpace1(int width);

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
  void UpdateWellHeightFromBottom(int p_well_height, int n_well_height);
  void UpdateWellHeightFromTop(int p_well_height, int n_well_height);
  int Height() const;
  int PHeight() const;
  int NHeight() const;
  int PNEdge() const;
  void UpdateWellHeightFromBottom1(int p_well_height, int n_well_height);
  void UpdateWellHeightFromTop1(int p_well_height, int n_well_height);
  int PHeight1() const;
  int NHeight1() const;
  int PNEdge1() const;
  int ClusterEdge() const;

  void SetLoc(int lx, int ly);

  void AddBlock(Block *blk_ptr);
  void AddBlock1(Block *blk_ptr);
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

  void DoubleWellLegalization();
 private:
  bool is_orient_N_ = true; // orientation of this cluster
  bool is_only_single_well_cells_ = true;
  std::vector<Block *> blk_list_; // list of blocks in this cluster
  int cluster_boundary_ = 0;
  std::vector<Block *> blk_list1_;
  std::vector<Block *> blk_list_double_;
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
  int used_size1_ = 0;
  int usable_width_; // to ensure a proper well tap cell location can be found

  /**** maximum p-well height and n-well height ****/
  int p_well_height_ = 0;
  int n_well_height_ = 0;
  int height_ = 0;
  int n_well_height1_ = 0;
  int p_well_height1_ = 0;
  int height1_ = 0;

  /**** lly which gives minimal displacement ****/
  double min_displacement_lly_ = -DBL_MAX;
};

class ClusterSegment {
 private:
  int ly_;
  int height_;
 public:
  ClusterSegment(Cluster *cluster_ptr, int loc)
      : ly_(loc), height_(cluster_ptr->Height()) {
    cluster_list.push_back(cluster_ptr);
  }
  std::vector<Cluster *> cluster_list;

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

#endif //DALI_DALI_PLACER_WELLLEGALIZER_CLUSTER_H_
