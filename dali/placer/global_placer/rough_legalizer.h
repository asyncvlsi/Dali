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

class RoughLegalizer {
 public:
  explicit RoughLegalizer(Circuit *ckt_ptr);
  virtual ~RoughLegalizer() = default;
  virtual void Initialize(double placement_density) = 0;
  virtual double RemoveCellOverlap() = 0;
  virtual double GetTime() = 0;
  virtual void Close() = 0;
  std::vector<double> &GetHpwls() { return upper_bound_hpwl_; }
  std::vector<double> &GetHpwlsX() { return upper_bound_hpwl_x_; }
  std::vector<double> &GetHpwlsY() { return upper_bound_hpwl_y_; }
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);
 protected:
  Circuit *ckt_ptr_ = nullptr;
  double placement_density_ = 1.0;
  std::vector<double> upper_bound_hpwl_;
  std::vector<double> upper_bound_hpwl_x_;
  std::vector<double> upper_bound_hpwl_y_;

  // save intermediate result for debugging and/or visualization
  bool should_save_intermediate_result_ = false;
  int cur_iter_ = 0;
};

class LookAheadLegalizer : public RoughLegalizer {
 public:
  explicit LookAheadLegalizer(Circuit *ckt_ptr) : RoughLegalizer(ckt_ptr) {}
  ~LookAheadLegalizer() override = default;

  void InitializeGridBinSize();
  void UpdateAttributesForAllGridBins();
  void UpdateFixedBlocksInGridBins();
  void UpdateDummyPlacementBlockagesInGridBins();
  void UpdateWhiteSpaceInGridBin(GridBin &grid_bin);
  void InitGridBins();
  void InitWhiteSpaceLUT();
  void Initialize(double placement_density) override;

  void ClearGridBinFlag();
  void UpdateGridBinState();
  void UpdateClusterArea(GridBinCluster &cluster);
  void UpdateClusterList();
  void UpdateLargestCluster();
  uint32_t LookUpWhiteSpace(
      GridBinIndex const &ll_index,
      GridBinIndex const &ur_index
  );
  uint32_t LookUpWhiteSpace(WindowQuadruple &window);
  void FindMinimumBoxForLargestCluster();
  void SplitGridBox(BoxBin &box);
  void PlaceBlkInBox(BoxBin &box);
  void SplitBox(BoxBin &box);
  bool RecursiveBisectionBlockSpreading();
  double RemoveCellOverlap() override;

  double GetTime() override;
  void Close() override;
 private:
  int number_of_cell_in_bin_ = 30;
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
  double recursive_bisection_block_spreading_time_ = 0;
  double tot_lal_time = 0;
};

} // dali

#endif //DALI_PLACER_GLOBAL_PLACER_ROUGH_LEGALIZER_H_
