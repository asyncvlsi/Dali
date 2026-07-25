/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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
#ifndef DALI_PLACER_GLOBAL_PLACER_LOOK_AHEAD_SPREADER_H_
#define DALI_PLACER_GLOBAL_PLACER_LOOK_AHEAD_SPREADER_H_

#include <memory>
#include <queue>
#include <set>
#include <stdint.h>

#include "dali/placer/global_placer/global_spreader.h"
#include "dali/placer/global_placer/grid_bin.h"
#include "dali/placer/global_placer/placement_capacity_model.h"
#include "dali/placer/global_placer/spreading_region.h"

namespace dali {

/** How LAL expands overfilled clusters into legal whitespace regions. */
enum class GlobalLalExpansionMode {
  kSymmetric,
  kBestNeighbor,
};

/** How LAL chooses the next overfilled cluster to spread. */
enum class GlobalLalHotspotMode {
  kComponentArea,
  kOverflow,
  kOverflowRatio,
};

/** Look-ahead legalization using grid bins and recursive bisection spreading.
 */
class LookAheadSpreader : public GlobalSpreader {
 public:
  LookAheadSpreader(
      Circuit* circuit,
      std::shared_ptr<const PlacementCapacityModel> capacity_model);
  ~LookAheadSpreader() override = default;

  /** Select how overfilled clusters expand into whitespace. */
  void SetExpansionMode(GlobalLalExpansionMode mode) { expansion_mode_ = mode; }

  /** Select how the next overfilled hotspot is ranked. */
  void SetHotspotMode(GlobalLalHotspotMode mode) { hotspot_mode_ = mode; }

  /** Set the affine geometry-preservation weight used in leaf boxes. */
  void SetAffineScalingWeight(double weight) {
    affine_scaling_weight_ = weight;
  }

  /** Select whether macro boundaries influence LAL cutlines. */
  void SetMacroBoundaryMode(GlobalLalMacroBoundaryMode mode) {
    macro_boundary_mode_ = mode;
  }

  void InitializeGridBinSize();
  void UpdateAttributesForAllGridBins();
  void UpdatePlacementBlockagesInGridBins();
  void UpdateDummyPlacementBlockagesInGridBins();
  /** Recompute one bin's free area after blockages and fixed cells. */
  void UpdateWhiteSpaceInGridBin(GridBin& grid_bin);
  void InitGridBins();
  void InitWhiteSpaceLUT();
  /** Build the density grid and white-space tables for a placement density. */
  void Initialize(double placement_density) override;
  int TargetComponentCountPerBin() const;
  void RebuildGridBinsIfTargetChanged();

  void ClearGridBinFlag();
  void UpdateGridBinState();
  PlacementCapacity EvaluateWindow(const GridBinIndex& lower_left,
                                   const GridBinIndex& upper_right,
                                   unsigned long long whitespace_area,
                                   CapacityEvaluationPurpose purpose) const;
  /** Recompute how much component area a spreading region can hold. */
  void UpdateRegionCapacity(SpreadingRegion* region) const;
  /** Recompute the total component area in an overfilled cluster. */
  void UpdateClusterArea(OverfilledBinCluster& cluster);
  void UpdateClusterList();
  std::multiset<OverfilledBinCluster, std::greater<>>::iterator
  SelectHotspotCluster();
  /** Rank an overfilled cluster for how urgently it needs relief. */
  double HotspotScore(const OverfilledBinCluster& cluster) const;
  static const char* HotspotModeName(GlobalLalHotspotMode mode);
  void UpdateLargestCluster();
  /** Total white space in a window of bins, from the prefix-sum table. */
  uint32_t LookUpWhiteSpace(GridBinIndex const& ll_index,
                            GridBinIndex const& ur_index) const;
  /** White space in a bin window (overload taking an explicit window). */
  uint32_t LookUpWhiteSpace(GridBinWindow& window) const;
  /** Grow a spreading box by the neighbour adding the most white space per area.
   * @return true if a neighbour was absorbed. */
  bool ExpandBoxByBestNeighbor(SpreadingRegion* box);
  void FindMinimumBoxForLargestCluster();
  /** Divide a box along the grid into two child boxes. */
  void SplitGridBox(SpreadingRegion& box);
  /** Distribute a box's components across its area by white space. */
  void PlaceComponentInBox(SpreadingRegion& box);
  /** Split a box's components between its halves in proportion to white space. */
  void SplitBox(SpreadingRegion& box);
  bool RecursiveBisectionComponentSpreading();
  double Spread() override;

  double GetTime() const override;
  void Close() override;

 private:
  /** Debug summary for the LAL hotspot chosen in the current spreading step. */
  struct HotspotDebugInfo {
    int bin_count = 0;
    unsigned long long component_area = 0;
    unsigned long long white_space = 0;
    double overflow = 0;
    double overflow_ratio = 0;
    double score = 0;
    GridBinIndex cluster_ll;
    GridBinIndex cluster_ur;
    GridBinIndex region_ll;
    GridBinIndex region_ur;
    double region_filling_rate = 0;
  };

  int target_component_count_per_bin_ = 30;
  int active_target_component_count_per_bin_ = 0;
  int cluster_upper_size = 3;

  // look ahead legalization member function implemented below
  int grid_bin_height = 0;
  int grid_bin_width = 0;
  int grid_cnt_x = 0;
  int grid_cnt_y = 0;
  std::vector<std::vector<GridBin>> grid_bin_mesh;
  std::vector<std::vector<unsigned long long>> grid_bin_white_space_LUT;

  std::multiset<OverfilledBinCluster, std::greater<>> cluster_set;
  std::queue<SpreadingRegion> spreading_region_queue_;

  double update_grid_bin_state_time_ = 0;
  double cluster_overfilled_grid_bin_time_ = 0;
  double update_cluster_area_time_ = 0;
  double update_cluster_list_time_ = 0;
  double find_minimum_box_for_largest_cluster_time_ = 0;
  double recursive_bisection_component_spreading_time_ = 0;
  double tot_lal_time = 0;

  int last_overfilled_bin_count_ = 0;
  double last_peak_bin_density_ = 0.0;
  double last_hpwl_before_ = 0.0;
  double last_hpwl_after_ = 0.0;
  int last_hotspot_count_ = 0;
  double last_max_hotspot_overflow_ = 0.0;
  HotspotDebugInfo last_hotspot_debug_;
  std::shared_ptr<const PlacementCapacityModel> capacity_model_;

  GlobalLalExpansionMode expansion_mode_ = GlobalLalExpansionMode::kSymmetric;
  GlobalLalHotspotMode hotspot_mode_ = GlobalLalHotspotMode::kComponentArea;
  double affine_scaling_weight_ = 0.65;
  GlobalLalMacroBoundaryMode macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_LOOK_AHEAD_SPREADER_H_
