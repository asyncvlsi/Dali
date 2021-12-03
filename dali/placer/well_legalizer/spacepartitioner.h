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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "stripe.h"

namespace dali {

class SpacePartitioner {
 public:
  SpacePartitioner() = default;

  void SetCircuit(Circuit *p_ckt);
  void SetOutput(std::vector<ClusterStripe> *p_col_list);
  void SetPartitionMode(StripePartitionMode stripe_mode);

  void SetReservedSpaceToBoundaries(
      int l_space, int r_space, int b_space, int t_space
  );

  void FetchWellParameters();
  void DetectAvailSpace();
  void UpdateWhiteSpaceInCol(ClusterStripe &col);
  void DecomposeSpaceToSimpleStripes();
  void AssignBlockToColBasedOnWhiteSpace();

  bool StartPartitioning();

  /**** member functions for debugging ****/
  void PlotAvailSpace(std::string const &name_of_file = "avail_space.txt");
  void PlotAvailSpaceInCols(std::string const &name_of_file = "avail_space.txt");
  void PlotSimpleStripes(std::string const &name_of_file = "stripe_space.txt");
 private:
  Circuit *p_ckt_ = nullptr;

  // some distances reserved to every edge
  int l_space_ = 0;
  int r_space_ = 0;
  int b_space_ = 0;
  int t_space_ = 0;

  /**** well parameters ****/
  int max_unplug_length_ = 0;
  int well_spacing_ = 0;

  /**** stripe parameters ****/
  int max_cell_width_ = 0;
  double stripe_width_factor_ = 2.0;
  StripePartitionMode stripe_mode_ = STRICT;

  // write result to an external container in a legalizer
  int cluster_width_ = 0;
  int tot_col_num_ = 0;
  int stripe_width_ = 0;
  std::vector<ClusterStripe> *p_col_list_ = nullptr;

  /**** row information ****/
  bool row_height_set_ = false;
  int row_height_ = 0;
  int tot_num_rows_ = 0;
  std::vector<std::vector<SegI>> white_space_in_rows_;

  int Left() const;
  int Right() const;
  int Bottom() const;
  int Top() const;

  int StartRow(int y_loc) const;
  int EndRow(int y_loc) const;
  int RowToLoc(int row_num, int displacement = 0) const;
  int LocToCol(int x) const;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_
