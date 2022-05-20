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

#include "dali/circuit/circuit.h"
#include "grid_bin.h"

namespace dali {

class RoughLegalizer {
 public:
  explicit RoughLegalizer(Circuit *ckt_ptr);
  virtual ~RoughLegalizer() = default;
  virtual void Initialize(double placement_density) = 0;
  virtual double LookAheadLegalization() = 0;
 protected:
  Circuit *ckt_ptr_ = nullptr;
  double placement_density_ = 1.0;
};

class LookAheadLegalizer : public RoughLegalizer {
 public:
  explicit LookAheadLegalizer(Circuit *ckt_ptr) : RoughLegalizer(ckt_ptr) {}
  ~LookAheadLegalizer() override = default;

  void InitializeGridBinSize();
  void UpdateAttributesForAllGridBins();
  void UpdateFixedBlocksInGridBins();
  void UpdateWhiteSpaceInGridBin(GridBin &grid_bin);
  void InitGridBins();
  void InitWhiteSpaceLUT();
  void Initialize(double placement_density) override;
/*
  void BackUpBlockLocation();
  void ClearGridBinFlag();
  void UpdateGridBinState();
  void UpdateClusterList();*/
  double LookAheadLegalization() override;
 private:
  std::vector<double> upper_bound_hpwlx_;
  std::vector<double> upper_bound_hpwly_;
  std::vector<double> upper_bound_hpwl_;

  int number_of_cell_in_bin_ = 30;

  // look ahead legalization member function implemented below
  int grid_bin_height = 0;
  int grid_bin_width = 0;
  int grid_cnt_x = 0;
  int grid_cnt_y = 0;
  std::vector<std::vector<GridBin> > grid_bin_mesh;
  std::vector<std::vector<unsigned long long> > grid_bin_white_space_LUT;
};

} // dali

#endif //DALI_PLACER_GLOBAL_PLACER_ROUGH_LEGALIZER_H_
