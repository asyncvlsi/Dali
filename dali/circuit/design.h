//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_DALI_CIRCUIT_DESIGN_H_
#define DALI_DALI_CIRCUIT_DESIGN_H_

#include <climits>

#include <map>
#include <vector>

#include "block.h"
#include "iopin.h"
#include "net.h"

namespace dali {

struct NetHistogram {
  std::vector<int> bin_list_{2, 3, 4, 20, 40, 80, 160};
  std::vector<int> count_;
  std::vector<double> percent_;
  std::vector<double> sum_hpwl_;
  std::vector<double> ave_hpwl_;
  std::vector<double> min_hpwl_;
  std::vector<double> max_hpwl_;
  int tot_net_count_;
  double tot_hpwl_;
  double hpwl_unit_;
};

struct Design {
  /****design name****/
  std::string name_;

  /****def distance microns****/
  int def_distance_microns = 0;

  /****die area****/
  int region_left_ = 0; // unit is grid value x
  int region_right_ = 0; // unit is grid value x
  int region_bottom_ = 0; // unit is grid value y
  int region_top_ = 0; // unit is grid value y
  bool die_area_set_ = false;

  int die_area_offset_x_ = 0; // unit is manufacturing grid
  int die_area_offset_y_ = 0; // unit is manufacturing grid

  /****list of instances****/
  std::vector<Block> block_list; // block list consists of blocks and dummy blocks for pre-placed IOPINs
  std::map<std::string, int> block_name_map;
  std::vector<Block> well_tap_list;
  std::map<std::string, int> tap_name_map;
  int blk_count_ = 0; // number of blocks added by calling the AddBlock() API
  int def_blk_count_ = 0; // number of blocks given in DEF, these two numbers are supposed to be the same

  /****list of IO Pins****/
  std::vector<IOPin> iopin_list;
  std::map<std::string, int> iopin_name_map;
  int pre_placed_io_count_ = 0;
  int iopin_count_ = 0;
  int def_iopin_count_ = 0;

  /****list of nets****/
  double reset_signal_weight = 1;
  double normal_signal_weight = 1;
  std::vector<Net> net_list;
  int net_count_ = 0;
  int def_net_count_ =0;
  std::map<std::string, int> net_name_map;
  NetHistogram net_histogram_;

  /****statistical data of the circuit****/
  long int tot_width_ = 0;
  long int tot_height_ = 0;
  long int tot_blk_area_ = 0;
  long int tot_mov_width_ = 0;
  long int tot_mov_height_ = 0;
  long int tot_mov_block_area_ = 0;
  int tot_mov_blk_num_ = 0;
  int blk_min_width_ = INT_MAX;
  int blk_max_width_ = INT_MIN;
  int blk_min_height_ = INT_MAX;
  int blk_max_height_ = INT_MIN;

  void UpdateFanoutHisto(int net_size);
  void InitNetFanoutHisto(std::vector<int> *histo_x = nullptr);
  void UpdateNetHPWLHisto(int net_size, double hpwl);
  void ReportNetFanoutHisto();
};

}

#endif //DALI_DALI_CIRCUIT_DESIGN_H_
