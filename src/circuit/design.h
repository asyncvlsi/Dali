//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_CIRCUIT_DESIGN_H_
#define DALI_SRC_CIRCUIT_DESIGN_H_

#include <climits>

#include <map>
#include <vector>

#include "block.h"
#include "iopin.h"
#include "net.h"

class Design {
 public:
  /****def distance microns****/
  int def_distance_microns = 0;

  /****die area****/
  int def_left = 0;
  int def_right = 0;
  int def_bottom = 0;
  int def_top = 0;

  /****list of instances****/
  std::vector<Block> block_list;
  std::map<std::string, int> block_name_map;
  std::vector<Block> well_tap_list;
  std::map<std::string, int> tap_name_map;

  /****list of IO Pins****/
  std::vector<IOPin> iopin_list;
  std::map<std::string, int> iopin_name_map;

  /****list of nets****/
  double reset_signal_weight = 1;
  double normal_signal_weight = 1;
  std::vector<Net> net_list;
  std::map<std::string, int> net_name_map;

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
};

#endif //DALI_SRC_CIRCUIT_DESIGN_H_
