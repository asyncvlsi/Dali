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
#ifndef DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_
#define DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "stripe.h"

namespace dali {

/****
 * This is a base class for SpacePartitioner. This partitioner is used by well
 * legalizers to clean up design rule checking errors related to N/P-wells.
 *
 * A sample code snippet in a well legalizer is like the following:
 *   p_partitioner->SetInputCircuit(p_ckt);
 *   p_partitioner->SetOutput(&col_list);
 *   p_partitioner->SetReservedSpaceToBoundaries(l_space, r_space, b_space, t_space);
 *   p_partitioner->SetPartitionMode(partition_mode);
 *   p_partitioner->StartPartitioning();
 *
 * After partitioning, the whole placement region and circuit is partitioned into
 * several smaller rectangular placement regions and sub-circuits. The partitioning
 * results are saved in the output container. Then the well legalizer will take
 * over from here, and perform legalization is each rectangular placement region.
 */
class AbstractSpacePartitioner {
 public:
  AbstractSpacePartitioner() = default;
  virtual ~AbstractSpacePartitioner() = default;

  virtual void SetInputCircuit(Circuit *p_ckt);
  virtual void SetOutput(std::vector<ClusterStripe> *p_col_list);
  virtual void SetReservedSpaceToBoundaries(
      int l_space, int r_space, int b_space, int t_space
  );
  virtual void SetPartitionMode(int partition_mode);
  virtual void SetMaxRowWidth(int max_row_width);

  virtual bool StartPartitioning() = 0;
 protected:
  Circuit *p_ckt_ = nullptr;
  std::vector<ClusterStripe> *p_col_list_ = nullptr;

  // some distances reserved to every edge
  int l_space_ = 0;
  int r_space_ = 0;
  int b_space_ = 0;
  int t_space_ = 0;

  int partition_mode_ = 0;
  int max_row_width_ = -1;
};

enum class DefaultPartitionMode {
  STRICT = 0,
  SCAVENGE = 1
};

/****
 * The default space partitioner. If no external space partitioner is provided
 * during well legalization, the well legalizer will use this default space
 * partitioner to partition placement region and circuit.
 */
class DefaultSpacePartitioner : public AbstractSpacePartitioner {
 public:
  DefaultSpacePartitioner() = default;
  ~DefaultSpacePartitioner() override = default;

  void FetchWellParameters();
  void DetectAvailSpace();
  void UpdateWhiteSpaceInCol(ClusterStripe &col);
  void DecomposeSpaceToSimpleStripes();
  void AssignBlockToColBasedOnWhiteSpace();

  bool StartPartitioning() override;

  /**** member functions for debugging ****/
  void PlotAvailSpace(std::string const &name_of_file = "avail_space.txt");
  void PlotAvailSpaceInCols(std::string const &name_of_file = "avail_space.txt");
  void PlotSimpleStripes(std::string const &name_of_file = "stripe_space.txt");
 private:
  /**** well parameters ****/
  int max_unplug_length_ = 0;
  int well_spacing_ = 0;

  /**** stripe parameters ****/
  int max_cell_width_ = 0;
  double stripe_width_factor_ = 2.0;

  /**** write result to an external container in a legalizer ****/
  int cluster_width_ = 0;
  int tot_col_num_ = 0;
  int stripe_width_ = 0;

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

#endif //DALI_PLACER_WELL_LEGALIZER_SPACEPARTITIONER_H_
