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

struct NetHistogram {
  std::vector<int> fanout_x_{2, 3, 4, 20, 40, 80, 160};
  std::vector<int> fanout_y_;
  std::vector<double> fanout_percent_;
  std::vector<double> fanout_hpwl_;
  std::vector<double> fanout_hpwl_per_pin_; // (HPWL of a net)/(size of a net)
  int tot_net_count_;
  double tot_hpwl_;
};

class Design {
 public:
  /****def distance microns****/
  int def_distance_microns = 0;

  /****die area****/
  int def_left = 0; // unit is grid value x
  int def_right = 0; // unit is grid value x
  int def_bottom = 0; // unit is grid value y
  int def_top = 0; // unit is grid value y

  int die_area_offset_x = 0; // unit is manufacturing grid
  int die_area_offset_y = 0; // unit is manufacturing grid

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

#endif //DALI_SRC_CIRCUIT_DESIGN_H_
