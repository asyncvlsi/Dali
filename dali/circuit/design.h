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
#ifndef DALI_CIRCUIT_DESIGN_H_
#define DALI_CIRCUIT_DESIGN_H_

#include <climits>

#include <unordered_map>
#include <vector>

#include "block.h"
#include "iopin.h"
#include "net.h"

namespace dali {

/****
 * This is a struct for getting the statistics of net size and wire-length.
 */
struct NetHistogram {
  std::vector<size_t> buckets{2, 3, 4, 20, 40, 80, 160}; // this list defines the buckets for net size
  std::vector<size_t> counts;                            // the number of nets in each bucket
  std::vector<double> percents;                          // the percentage of nets in each bucket
  std::vector<double> sum_hpwls;
  std::vector<double> ave_hpwls;
  std::vector<double> min_hpwls;
  std::vector<double> max_hpwls;
  size_t tot_net_count;
  double tot_hpwl;
  double hpwl_unit;
};

/****
 * This class contain basic information of a design (or information in a DEF file)
 */
class Design {
  friend class Circuit;
 public:
  // get the name of this design
  const std::string &Name() const { return name_; }

  // get DEF distance microns
  int32_t DistanceMicrons() const { return distance_microns_; }

  // get the location of placement boundaries
  int32_t RegionLeft() const { return region_left_; }
  int32_t RegionRight() const { return region_right_; }
  int32_t RegionBottom() const { return region_bottom_; }
  int32_t RegionTop() const { return region_top_; }

  // locations of cells in Dali is on grid, but they do not have to be on grid
  // in a DEF file. The following two APIs return the offset along each direction.
  int32_t DieAreaOffsetX() const { return die_area_offset_x_; }
  int32_t DieAreaOffsetY() const { return die_area_offset_y_; }
  int32_t DieAreaOffsetXResidual() const { return die_area_offset_x_residual_; }
  int32_t DieAreaOffsetYResidual() const { return die_area_offset_y_residual_; }

  // get all blocks
  std::vector<Block> &Blocks() { return blocks_; }

  // some blocks are imaginary (for example, fixed IOPINs), this function
  // returns the number of real blocks (for example, gates)
  int32_t RealBlkCnt() const { return real_block_count_; }

  // get all well tap cells
  std::vector<Block> &WellTaps() { return welltaps_; }

  // get well tap cell name-id map
  std::unordered_map<std::string, int32_t> &TapNameIdMap() {
    return tap_name_id_map_;
  };

  // get all iopins
  std::vector<IoPin> &IoPins() { return iopins_; }

  // get all nets
  std::vector<Net> &Nets() { return nets_; }

  void UpdateFanOutHistogram(size_t net_size);
  void InitNetFanOutHistogram(std::vector<size_t> *histo_x = nullptr);
  void UpdateNetHPWLHistogram(size_t net_size, double hpwl);
  void ReportNetFanOutHistogram();
 private:
  /****design name****/
  std::string name_;

  /****def distance microns****/
  int32_t distance_microns_ = 0;

  /****die area****/
  int32_t region_left_ = 0; // unit is grid value x
  int32_t region_right_ = 0; // unit is grid value x
  int32_t region_bottom_ = 0; // unit is grid value y
  int32_t region_top_ = 0; // unit is grid value y
  bool die_area_set_ = false;

  // if left boundary is not on grid, then how much distance should we shift it to make it on grid
  int32_t die_area_offset_x_ = 0; // unit is manufacturing grid
  // if right boundary is still not on grid after shifting, this number is the residual
  int32_t die_area_offset_x_residual_ = 0; // unit is manufacturing grid
  // if bottom boundary is not on grid, then how much distance should we shift it to make it on grid
  int32_t die_area_offset_y_ = 0; // unit is manufacturing grid
  // if top boundary is still not on grid after shifting, this number is the residual
  int32_t die_area_offset_y_residual_ = 0; // unit is manufacturing grid

  /****list of instances****/
  // block list consists of blocks and dummy blocks for pre-placed IOPINs
  std::vector<Block> blocks_;
  std::unordered_map<std::string, int32_t> blk_name_id_map_;
  std::vector<Block> welltaps_;
  std::unordered_map<std::string, int32_t> tap_name_id_map_;
  // number of blocks added by calling the AddBlock() API
  int32_t real_block_count_ = 0;
  // number of blocks given in DEF, these two numbers are supposed to be the same
  int32_t blk_count_limit_ = 0;

  /****list of IO Pins****/
  std::vector<IoPin> iopins_;
  std::unordered_map<std::string, int32_t> iopin_name_id_map_;
  int32_t pre_placed_io_count_ = 0;
  int32_t added_iopin_count_ = 0;
  int32_t iopin_count_limit_ = 0;

  /****list of nets****/
  double reset_signal_weight_ = 1;
  double normal_signal_weight_ = 1;
  std::vector<Net> nets_;
  int32_t added_net_count_ = 0;
  int32_t net_count_limit_ = 0;
  std::unordered_map<std::string, int32_t> net_name_id_map_;
  NetHistogram net_histogram_;

  /****statistical data of the circuit****/
  unsigned long tot_width_ = 0;
  unsigned long tot_height_ = 0;
  unsigned long long tot_blk_area_ = 0;
  unsigned long long tot_fixed_blk_cover_area_ = 0;
  unsigned long long tot_white_space_ = 0;
  unsigned long tot_mov_width_ = 0;
  unsigned long tot_mov_height_ = 0;
  unsigned long long tot_mov_blk_area_ = 0;
  int32_t tot_mov_blk_num_ = 0;
  int32_t tot_fixed_blk_num_ = 0;
  int32_t blk_min_width_ = INT_MAX;
  int32_t blk_max_width_ = INT_MIN;
  int32_t blk_min_height_ = INT_MAX;
  int32_t blk_max_height_ = INT_MIN;
};

}

#endif //DALI_CIRCUIT_DESIGN_H_
