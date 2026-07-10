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
#ifndef DALI_PLACER_GLOBAL_PLACER_ROUGH_LEGALIZER_H_
#define DALI_PLACER_GLOBAL_PLACER_ROUGH_LEGALIZER_H_

#include <queue>
#include <set>

#include "dali/circuit/circuit.h"
#include "dali/placer/global_placer/box_bin.h"
#include "dali/placer/global_placer/grid_bin.h"

namespace dali {

/** Grid refinement schedule used by look-ahead legalization. */
enum class GlobalGridSchedule {
  kDali,
  kSimpl,
};

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

/** Interface for rough legalizers that remove gross component overlap. */
class RoughLegalizer {
 public:
  explicit RoughLegalizer(Circuit* ckt_ptr);
  virtual ~RoughLegalizer() = default;

  /** Initialize legalizer state for a target placement density. */
  virtual void Initialize(double placement_density) = 0;

  /** Spread components to reduce overlap and return current HPWL. */
  virtual double RemoveComponentOverlap() = 0;

  /** Return total legalizer runtime in seconds. */
  virtual double GetTime() = 0;

  /** Release legalizer resources. */
  virtual void Close() = 0;

  /** Return upper-bound HPWL history. */
  std::vector<double>& GetHpwls() { return upper_bound_hpwl_; }

  /** Return x upper-bound HPWL history. */
  std::vector<double>& GetHpwlsX() { return upper_bound_hpwl_x_; }

  /** Return y upper-bound HPWL history. */
  std::vector<double>& GetHpwlsY() { return upper_bound_hpwl_y_; }

  /** Enable or disable intermediate placement dumps. */
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);

  /** Update the current global-placement iteration. */
  void SetIteration(int iteration) { cur_iter_ = iteration; }

  /** Select how look-ahead legalization grid dimensions are refined. */
  void SetGridSchedule(GlobalGridSchedule schedule) {
    grid_schedule_ = schedule;
  }

  /** Select how overfilled LAL clusters expand into whitespace. */
  void SetExpansionMode(GlobalLalExpansionMode mode) { expansion_mode_ = mode; }

  /** Select how the next overfilled LAL hotspot is ranked. */
  void SetHotspotMode(GlobalLalHotspotMode mode) { hotspot_mode_ = mode; }

  /** Select whether macro boundaries influence LAL cutlines. */
  void SetMacroBoundaryMode(GlobalLalMacroBoundaryMode mode) {
    macro_boundary_mode_ = mode;
  }

 protected:
  Circuit* ckt_ptr_ = nullptr;
  double placement_density_ = 1.0;
  std::vector<double> upper_bound_hpwl_;
  std::vector<double> upper_bound_hpwl_x_;
  std::vector<double> upper_bound_hpwl_y_;

  // Save intermediate result for debugging and/or visualization.
  bool should_save_intermediate_result_ = false;
  int cur_iter_ = 0;
  GlobalGridSchedule grid_schedule_ = GlobalGridSchedule::kDali;
  GlobalLalExpansionMode expansion_mode_ = GlobalLalExpansionMode::kSymmetric;
  GlobalLalHotspotMode hotspot_mode_ = GlobalLalHotspotMode::kComponentArea;
  GlobalLalMacroBoundaryMode macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
};

/** Look-ahead legalization using grid bins and recursive bisection spreading.
 */
class LookAheadLegalizer : public RoughLegalizer {
 public:
  explicit LookAheadLegalizer(Circuit* ckt_ptr) : RoughLegalizer(ckt_ptr) {}
  ~LookAheadLegalizer() override = default;

  void InitializeGridBinSize();
  void UpdateAttributesForAllGridBins();
  void UpdatePlacementBlockagesInGridBins();
  void UpdateDummyPlacementBlockagesInGridBins();
  void UpdateWhiteSpaceInGridBin(GridBin& grid_bin);
  void InitGridBins();
  void InitWhiteSpaceLUT();
  void Initialize(double placement_density) override;
  int TargetComponentCountPerBin() const;
  void RebuildGridBinsIfTargetChanged();

  void ClearGridBinFlag();
  void UpdateGridBinState();
  void UpdateClusterArea(GridBinCluster& cluster);
  void UpdateClusterList();
  std::multiset<GridBinCluster, std::greater<>>::iterator
  SelectHotspotCluster();
  double HotspotScore(const GridBinCluster& cluster) const;
  static const char* HotspotModeName(GlobalLalHotspotMode mode);
  void UpdateLargestCluster();
  uint32_t LookUpWhiteSpace(GridBinIndex const& ll_index,
                            GridBinIndex const& ur_index);
  uint32_t LookUpWhiteSpace(WindowQuadruple& window);
  bool ExpandBoxByBestNeighbor(BoxBin* box);
  void FindMinimumBoxForLargestCluster();
  void SplitGridBox(BoxBin& box);
  void PlaceComponentInBox(BoxBin& box);
  void SplitBox(BoxBin& box);
  bool RecursiveBisectionComponentSpreading();
  double RemoveComponentOverlap() override;

  double GetTime() override;
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

  std::multiset<GridBinCluster, std::greater<>> cluster_set;
  std::queue<BoxBin> queue_box_bin;

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
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_ROUGH_LEGALIZER_H_
