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
#include "dali/common/named_instance_collection.h"
#include "diearea.h"
#include "iopin.h"
#include "net.h"
#include "placement_blockage.h"
#include "row.h"

namespace dali {

/** Net fanout and HPWL statistics grouped into configured fanout buckets. */
struct NetHistogram {
  // this list defines the buckets for net size
  std::vector<size_t> buckets{2, 3, 4, 20, 40, 80, 160};
  // the number of nets in each bucket
  std::vector<size_t> counts;
  // the percentage of nets in each bucket
  std::vector<double> percents;
  std::vector<double> sum_hpwls;
  std::vector<double> ave_hpwls;
  std::vector<double> min_hpwls;
  std::vector<double> max_hpwls;
  size_t tot_net_count;
  double tot_hpwl;
  double hpwl_unit;
};

/** DEF-side design data: instances, nets, rows, die area, and blockages. */
class Design {
  friend class Circuit;

 public:
  /** Return the design name. */
  std::string const& Name() const { return name_; }

  /** Return DEF UNITS DISTANCE MICRONS. */
  int DistanceMicrons() const { return distance_microns_; }

  /** Return left placement boundary in Dali grid units. */
  int RegionLeft() const { return die_area_.region_left_; }

  /** Return right placement boundary in Dali grid units. */
  int RegionRight() const { return die_area_.region_right_; }

  /** Return bottom placement boundary in Dali grid units. */
  int RegionBottom() const { return die_area_.region_bottom_; }

  /** Return top placement boundary in Dali grid units. */
  int RegionTop() const { return die_area_.region_top_; }

  /** Return x shift used to align off-grid DEF die area to Dali grid. */
  int DieAreaOffsetX() const { return die_area_.die_area_offset_x_; }

  /** Return y shift used to align off-grid DEF die area to Dali grid. */
  int DieAreaOffsetY() const { return die_area_.die_area_offset_y_; }

  /** Return x residual after aligning die area to Dali grid. */
  int DieAreaOffsetXResidual() const {
    return die_area_.die_area_offset_x_residual_;
  }

  /** Return y residual after aligning die area to Dali grid. */
  int DieAreaOffsetYResidual() const {
    return die_area_.die_area_offset_y_residual_;
  }

  /** Return all regular block instances. */
  std::vector<Block>& Blocks() { return block_collection_.Instances(); }

  /** Return regular block name-to-id lookup. */
  std::unordered_map<std::string, size_t>& BlockNameIdMap() {
    return block_collection_.NameToIdMap();
  }

  /** Return the number of real, user/design-created blocks. */
  int RealBlkCnt() const { return real_block_count_; }

  /** Return well tap cell instances. */
  std::vector<Block>& WellTaps() {
    return well_tap_cell_collection_.Instances();
  }

  /** Return well tap cell name-to-id lookup. */
  std::unordered_map<std::string, size_t>& TapNameIdMap() {
    return well_tap_cell_collection_.NameToIdMap();
  };

  /** Return filler cell instances. */
  std::vector<Block>& Fillers() { return filler_cell_collection_.Instances(); }

  /** Return filler cell name-to-id lookup. */
  std::unordered_map<std::string, size_t>& FillerNameIdMap() {
    return filler_cell_collection_.NameToIdMap();
  };

  /** Return regular block collection. */
  NamedInstanceCollection<Block>& BlockCollection() {
    return block_collection_;
  }

  /** Return well tap cell collection. */
  NamedInstanceCollection<Block>& WellTapCellCollection() {
    return well_tap_cell_collection_;
  }

  /** Return filler cell collection. */
  NamedInstanceCollection<Block>& FillerCellCollection() {
    return filler_cell_collection_;
  };

  /** Return end-cap cell collection. */
  NamedInstanceCollection<Block>& EndCapCellCollection() {
    return end_cap_cell_collection_;
  }

  /** Return all I/O pins. */
  std::vector<IoPin>& IoPins() { return iopins_; }

  /** Return all nets. */
  std::vector<Net>& Nets() { return nets_; }

  /** Return all placement rows. */
  std::vector<GeneralRow>& Rows() { return rows_; }

  /** Return die area model. */
  DieArea& GetDieArea() { return die_area_; }

  /** Add a user/DEF placement blockage. */
  void AddIntrinsicPlacementBlockage(double lx, double ly, double ux,
                                     double uy);

  /** Add a placement blockage covering a fixed block. */
  void AddFixedCellPlacementBlockage(Block& block);

  /** Refresh blockages implied by rectilinear die area. */
  void UpdateDieAreaPlacementBlockages();

  /** Rebuild the combined placement blockage list. */
  void UpdatePlacementBlockages();

  /** Return all active placement blockages. */
  [[nodiscard]] const std::vector<PlacementBlockage>& PlacementBlockages()
      const;

  void UpdateFanOutHistogram(size_t net_size);
  void InitNetFanOutHistogram(std::vector<size_t>* histo_x = nullptr);
  void UpdateNetHPWLHistogram(size_t net_size, double hpwl);
  void ReportNetFanOutHistogram();

 private:
  /****design name****/
  std::string name_;

  /****def distance microns****/
  int distance_microns_ = 0;

  /****die area****/
  DieArea die_area_;

  /****list of instances****/
  NamedInstanceCollection<Block> block_collection_;
  NamedInstanceCollection<Block> well_tap_cell_collection_;
  NamedInstanceCollection<Block> filler_cell_collection_;
  NamedInstanceCollection<Block> end_cap_cell_collection_;
  // number of blocks added by calling the AddBlock() API
  int real_block_count_ = 0;
  // number of blocks given in DEF, these two numbers are supposed to be the
  // same
  int blk_count_limit_ = 0;

  /****placement blockages****/
  std::vector<PlacementBlockage> intrinsic_blockages_;
  std::vector<PlacementBlockage> fixed_cell_blockages_;
  std::vector<PlacementBlockage> die_area_dummy_blockages_;
  std::vector<PlacementBlockage> all_blockages_;

  /****list of IO Pins****/
  std::vector<IoPin> iopins_;
  std::unordered_map<std::string, int> iopin_name_id_map_;
  int pre_placed_io_count_ = 0;
  int added_iopin_count_ = 0;
  int iopin_count_limit_ = 0;

  /****list of nets****/
  double reset_signal_weight_ = 1;
  double normal_signal_weight_ = 1;
  std::vector<Net> nets_;
  int added_net_count_ = 0;
  int net_count_limit_ = 0;
  std::unordered_map<std::string, int> net_name_id_map_;
  NetHistogram net_histogram_;

  /****rows***/
  std::vector<GeneralRow> rows_;

  /****statistical data of the circuit****/
  unsigned long tot_width_ = 0;
  unsigned long tot_height_ = 0;
  unsigned long long tot_blk_area_ = 0;
  unsigned long long total_blockage_cover_area_ = 0;
  unsigned long long tot_white_space_ = 0;
  unsigned long tot_mov_width_ = 0;
  unsigned long tot_mov_height_ = 0;
  unsigned long long tot_mov_blk_area_ = 0;
  int tot_mov_blk_num_ = 0;
  int tot_fixed_blk_num_ = 0;
  int blk_min_width_ = INT_MAX;
  int blk_max_width_ = INT_MIN;
  int blk_min_height_ = INT_MAX;
  int blk_max_height_ = INT_MIN;

  /****helper functions****/
  RectI ExpandOffGridPlacementBlockage(double lx, double ly, double ux,
                                       double uy);
};

}  // namespace dali

#endif  // DALI_CIRCUIT_DESIGN_H_
