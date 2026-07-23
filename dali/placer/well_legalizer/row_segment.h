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
#ifndef DALI_PLACER_WELL_LEGALIZER_ROW_SEGMENT_H_
#define DALI_PLACER_WELL_LEGALIZER_ROW_SEGMENT_H_

#include <climits>
#include <unordered_map>
#include <vector>

#include "dali/circuit/component.h"
#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/component_helper.h"
#include "dali/placer/well_legalizer/optimization_helper.h"

namespace dali {

/**
 * A contiguous run of usable whitespace within a gridded row.
 *
 * Blockages, and any taps already placed in the row, break the row into
 * segments. Each segment is legalized in X on its own, which is what lets the
 * displacement optimizers treat a row as several small ordered problems instead
 * of one wide one.
 */
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

  std::vector<ComponentRegion>& ComponentRegions();
  /** Record a component and which of its well regions this segment holds. */
  void AddComponentRegion(Component* component_ptr, int region_id);
  /** Place the segment's cells at minimum displacement, keeping order. */
  void MinDisplacementLegalization(bool use_init_loc);
  void SnapComponentsToPlacementGrid();

  void SetOptimalAnchorWeight(double weight);
  /** Clamp the segment's cells into its legal X interval. */
  void FitInRange(std::vector<ComponentDisplacementVariable>& vars);
  double DispCost(std::vector<ComponentDisplacementVariable>& vars, int l,
                  int r, bool is_linear);
  void FindBestLocalOrder(std::vector<ComponentDisplacementVariable>& res,
                          double& best_cost,
                          std::vector<ComponentDisplacementVariable>& vars,
                          int cur, int l, int r, double left_bound,
                          double right_bound, double gap, int range,
                          bool is_linear);
  void LocalReorder(std::vector<ComponentDisplacementVariable>& vars,
                    int range = 3, int omit = 0, bool is_linear = false);
  /** Alternative local-reorder pass over the segment's cells. */
  void LocalReorder2(std::vector<ComponentDisplacementVariable>& vars);
  /**
   * Place this segment's components to minimize displacement from where they
   * sat before legalization.
   *
   * Components are sorted by current X and solved in that order, so the result
   * keeps the relative ordering the placement already had. `lambda` trades
   * anchor pull against displacement cost. With `is_weighted_anchor`, a
   * multi-region cell whose sub-locations disagree is weighted up in
   * proportion to that disagreement, measured against the segment's average --
   * pulling the halves of a split cell back together. `is_reorder` allows a
   * final bounded local reordering pass.
   */
  std::vector<ComponentDisplacementVariable> OptimizeQuadraticDisplacement(
      double lambda, bool is_weighted_anchor, bool is_reorder);

  /** Linear-cost counterpart of OptimizeQuadraticDisplacement. */
  std::vector<ComponentDisplacementVariable> OptimizeLinearDisplacement(
      double lambda, bool is_weighted_anchor, bool is_reorder);

 private:
  // list of components in this segment
  std::vector<ComponentRegion> component_regions_;
  int lx_ = INT_MIN;
  int width_ = 0;
  int used_size_ = 0;

  /**** for iterative displacement optimization ****/
  double opt_anchor_weight_ = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ROW_SEGMENT_H_
