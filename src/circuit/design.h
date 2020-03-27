//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_CIRCUIT_DESIGN_H_
#define DALI_SRC_CIRCUIT_DESIGN_H_

#include <map>
#include <vector>

#include "block.h"
#include "iopin.h"
#include "net.h"

class Design {
 public:
  /****die area****/
  int def_left = 0;
  int def_right = 0;
  int def_bottom = 0;
  int def_top = 0;

  /****list of instances****/
  std::vector<Block> block_list;
  std::map<std::string, int> block_name_map;

  /****list of IO Pins****/
  std::vector<IOPin> iopin_list;
  std::map<std::string, int> iopin_name_map;

  /****list of nets****/
  std::vector<Net> net_list;
  std::map<std::string, int> net_name_map;
};

#endif //DALI_SRC_CIRCUIT_DESIGN_H_
