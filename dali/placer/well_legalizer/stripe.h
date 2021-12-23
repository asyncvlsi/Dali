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
#ifndef DALI_DALI_PLACER_WELLLEGALIZER_STRIPE_H_
#define DALI_DALI_PLACER_WELLLEGALIZER_STRIPE_H_

#include "griddedrow.h"
#include "dali/circuit/block.h"
#include "dali/common/misc.h"

namespace dali {

struct Stripe {
  int lx_;
  int ly_;
  int width_;
  int height_;
  int max_blk_capacity_per_cluster_;

  int contour_;
  int used_height_;
  int cluster_count_;
  GriddedRow *front_row_;
  int front_id_;
  std::vector<GriddedRow> gridded_rows_;
  bool is_bottom_up_ = false;

  int block_count_;
  std::vector<Block *> block_list_;

  bool is_first_row_orient_N_ = true;
  std::vector<RectI> well_rect_list_;

  int LLX() const { return lx_; }
  int LLY() const { return ly_; }
  int URX() const { return lx_ + width_; }
  int URY() const { return ly_ + height_; }
  int Width() const { return width_; }
  int Height() const { return height_; }

  bool HasNoRowsSpillingOut() const;

  void MinDisplacementAdjustment();

  void SortBlocksBasedOnLLY();
  void SortBlocksBasedOnURY();
  void SortBlocksBasedOnStretchedURY();
  void SortBlocksBasedOnYLocation(int criterion);

  void UpdateFrontClusterUpward(int p_height, int n_height);
  void SimplyAddFollowingClusters(Block *p_blk, bool is_upward);
  bool AddBlockToFrontCluster(Block *p_blk, bool is_upward);
  size_t FitBlocksToFrontSpaceUpward(size_t start_id, int current_iteration);
  void LegalizeFrontCluster();
  void UpdateRemainingClusters(int p_height, int n_height, bool is_upward);
  void UpdateBlockStretchLength();

  void UpdateFrontClusterDownward(int p_height, int n_height);
  size_t FitBlocksToFrontSpaceDownward(size_t start_id, int current_iteration);

  void UpdateBlockYLocation();
};

struct ClusterStripe {
  int lx_;
  int width_;

  int block_count_;
  std::vector<Block *> block_list_;

  std::vector<RectI> well_rect_list_;

  std::vector<std::vector<SegI>> white_space_; // white space in each row
  std::vector<Stripe> stripe_list_;

  int Width() const { return width_; }
  int LLX() const { return lx_; }
  int URX() const { return lx_ + width_; }
  Stripe *GetStripeMatchSeg(SegI seg, int y_loc);
  Stripe *GetStripeMatchBlk(Block *blk_ptr);
  Stripe *GetStripeClosestToBlk(Block *blk_ptr, double &distance);
  void AssignBlockToSimpleStripe();
};

}

#endif //DALI_DALI_PLACER_WELLLEGALIZER_STRIPE_H_
