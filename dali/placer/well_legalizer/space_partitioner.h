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
#ifndef DALI_PLACER_WELL_LEGALIZER_SPACE_PARTITIONER_H_
#define DALI_PLACER_WELL_LEGALIZER_SPACE_PARTITIONER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"
#include "stripe.h"

namespace dali {

/**
 * Partitions the placement region into rectangular stripes for well
 * legalization.
 *
 * The output StripeColumn list is consumed by well legalizers, which legalize
 * each rectangular sub-region independently.
 */
class SpacePartitioner {
 public:
  SpacePartitioner() = default;
  virtual ~SpacePartitioner() = default;

  /** Set the circuit to partition. */
  virtual void SetCircuit(Circuit* circuit);

  /** Set the output stripe container. */
  virtual void SetOutput(std::vector<StripeColumn>* output_stripes);

  /** Reserve whitespace along placement boundaries. */
  virtual void SetReservedSpaceToBoundaries(int l_space, int r_space,
                                            int b_space, int t_space);

  /** Set partitioning mode. */
  virtual void SetPartitionMode(int partition_mode);

  /** Set maximum row width for generated stripes. */
  virtual void SetMaxRowWidth(int max_row_width);

  /** Configure opt-in demand-aware nonuniform stripe boundaries. */
  virtual void SetAdaptiveStripeBoundaries(
      bool enable, const GriddedCapacityConfig& capacity_config);

  /** Set interpolation from uniform (0) to fully adaptive (1) boundaries. */
  virtual void SetAdaptiveBoundaryBlend(double blend);

  /** Use explicit column cutlines, or clear the override with an empty list. */
  virtual void SetColumnBoundaries(const std::vector<int>& boundaries);

  /** Run partitioning and populate the output stripe container. */
  virtual bool StartPartitioning() = 0;

 protected:
  Circuit* circuit_ = nullptr;
  std::vector<StripeColumn>* output_stripes_ = nullptr;

  // some distances reserved to every edge
  int l_space_ = 0;
  int r_space_ = 0;
  int b_space_ = 0;
  int t_space_ = 0;

  int partition_mode_ = 0;
  int max_row_width_ = -1;
  bool use_adaptive_boundaries_ = false;
  double adaptive_boundary_blend_ = 1.0;
  std::vector<int> column_boundaries_override_;
  GriddedCapacityConfig capacity_config_;
};

enum class WellPartitionMode { kStrict = 0, kScavenge = 1 };

/** Built-in space partitioner used by Dali's well legalizers. */
class WellSpacePartitioner : public SpacePartitioner {
 public:
  WellSpacePartitioner() = default;
  ~WellSpacePartitioner() override = default;

  void FetchWellParameters();
  void DetectAvailSpace();
  /** Recompute the free space in one stripe column. */
  void UpdateWhiteSpaceInCol(StripeColumn& col);
  void DecomposeSpaceToSimpleStripes();
  void AssignComponentToColBasedOnWhiteSpace();

  bool StartPartitioning() override;

 private:
  /**** well parameters ****/
  int max_unplug_length_ = 0;
  int well_spacing_ = 0;

  /**** stripe parameters ****/
  int max_component_width_ = 0;
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

  std::vector<int> PlanColumnBoundaries(int region_width) const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_SPACE_PARTITIONER_H_
